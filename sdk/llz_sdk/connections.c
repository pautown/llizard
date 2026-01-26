#include "llz_sdk_connections.h"

#include "hiredis.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// ============================================================================
// Redis Keys
// ============================================================================

// Uses the standard playback command queue for connection status requests
#define REDIS_KEY_PLAYBACK_CMD_Q      "system:playback_cmd_q"
#define REDIS_KEY_CONNECTIONS_PREFIX  "connections:"
#define REDIS_KEY_CONN_SPOTIFY        "connections:spotify"
#define REDIS_KEY_CONN_TIMESTAMP      "connections:timestamp"
#define REDIS_KEY_CONN_RESPONSE       "connections:response"

// ============================================================================
// Internal State
// ============================================================================

static redisContext *g_connCtx = NULL;
static LlzConnectionsState g_state;
static LlzConnectionsConfig g_config;
static float g_timeSinceLastCheck = 0.0f;
static bool g_initialized = false;
static bool g_autoCheckEnabled = true;

// Service name lookup table
static const char *g_serviceNames[LLZ_SERVICE_COUNT] = {
    [LLZ_SERVICE_SPOTIFY] = "spotify"
};

static const char *g_stateStrings[] = {
    [LLZ_CONN_STATUS_UNKNOWN] = "Unknown",
    [LLZ_CONN_STATUS_CONNECTED] = "Connected",
    [LLZ_CONN_STATUS_DISCONNECTED] = "Disconnected",
    [LLZ_CONN_STATUS_ERROR] = "Error",
    [LLZ_CONN_STATUS_CHECKING] = "Checking..."
};

// ============================================================================
// Redis Connection Helpers
// ============================================================================

static void llz_conn_disconnect(void)
{
    if (g_connCtx) {
        redisFree(g_connCtx);
        g_connCtx = NULL;
    }
}

static bool llz_conn_connect(void)
{
    llz_conn_disconnect();

    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 500000;

    g_connCtx = redisConnectWithTimeout("127.0.0.1", 6379, timeout);
    if (!g_connCtx || g_connCtx->err) {
        llz_conn_disconnect();
        return false;
    }

    redisSetTimeout(g_connCtx, timeout);
    return true;
}

static bool llz_conn_ensure_connection(void)
{
    if (g_connCtx) {
        return true;
    }
    return llz_conn_connect();
}

static redisReply *llz_conn_command(const char *format, ...)
{
    if (!llz_conn_ensure_connection()) return NULL;

    va_list args;
    va_start(args, format);
    redisReply *reply = redisvCommand(g_connCtx, format, args);
    va_end(args);

    if (!reply) {
        llz_conn_disconnect();
        if (!llz_conn_ensure_connection()) {
            return NULL;
        }
        va_start(args, format);
        reply = redisvCommand(g_connCtx, format, args);
        va_end(args);
    }

    return reply;
}

// ============================================================================
// Internal Functions
// ============================================================================

static void llz_conn_init_state(void)
{
    memset(&g_state, 0, sizeof(g_state));
    g_state.serviceCount = LLZ_SERVICE_COUNT;

    for (int i = 0; i < LLZ_SERVICE_COUNT; i++) {
        g_state.services[i].service = (LlzServiceType)i;
        g_state.services[i].state = LLZ_CONN_STATUS_UNKNOWN;
        strncpy(g_state.services[i].serviceName,
                g_serviceNames[i],
                LLZ_CONNECTION_SERVICE_NAME_MAX - 1);
        g_state.services[i].serviceName[LLZ_CONNECTION_SERVICE_NAME_MAX - 1] = '\0';
    }
}

static bool llz_conn_send_status_request(const char *service)
{
    if (!llz_conn_ensure_connection()) return false;

    long long ts = (long long)time(NULL);

    redisReply *reply;
    if (service) {
        // Request for specific service - goes through standard playback command queue
        reply = llz_conn_command(
            "LPUSH %s {\"action\":\"check_connection\",\"service\":\"%s\",\"timestamp\":%lld}",
            REDIS_KEY_PLAYBACK_CMD_Q,
            service,
            ts
        );
    } else {
        // Request for all services - goes through standard playback command queue
        reply = llz_conn_command(
            "LPUSH %s {\"action\":\"check_all_connections\",\"timestamp\":%lld}",
            REDIS_KEY_PLAYBACK_CMD_Q,
            ts
        );
    }

    if (!reply) return false;
    bool success = reply->type == REDIS_REPLY_INTEGER;
    freeReplyObject(reply);

    if (success) {
        g_state.lastRefresh = (int64_t)time(NULL);
        g_state.refreshInProgress = true;

        // Mark all requested services as CHECKING
        if (service) {
            for (int i = 0; i < LLZ_SERVICE_COUNT; i++) {
                if (strcmp(g_serviceNames[i], service) == 0) {
                    g_state.services[i].state = LLZ_CONN_STATUS_CHECKING;
                    break;
                }
            }
        } else {
            for (int i = 0; i < LLZ_SERVICE_COUNT; i++) {
                g_state.services[i].state = LLZ_CONN_STATUS_CHECKING;
            }
        }
    }

    return success;
}

static void llz_conn_read_status_from_redis(void)
{
    // Read Spotify status
    redisReply *reply = llz_conn_command("GET %s", REDIS_KEY_CONN_SPOTIFY);
    if (reply) {
        if (reply->type == REDIS_REPLY_STRING && reply->str) {
            LlzServiceStatus *status = &g_state.services[LLZ_SERVICE_SPOTIFY];

            if (strcmp(reply->str, "connected") == 0) {
                status->state = LLZ_CONN_STATUS_CONNECTED;
                status->error[0] = '\0';
            } else if (strcmp(reply->str, "disconnected") == 0) {
                status->state = LLZ_CONN_STATUS_DISCONNECTED;
                status->error[0] = '\0';
            } else if (strncmp(reply->str, "error:", 6) == 0) {
                status->state = LLZ_CONN_STATUS_ERROR;
                strncpy(status->error, reply->str + 6, LLZ_CONNECTION_ERROR_MAX - 1);
                status->error[LLZ_CONNECTION_ERROR_MAX - 1] = '\0';
            }

            status->lastUpdated = (int64_t)time(NULL);
            g_state.refreshInProgress = false;
        }
        freeReplyObject(reply);
    }

    // Read timestamp
    reply = llz_conn_command("GET %s", REDIS_KEY_CONN_TIMESTAMP);
    if (reply) {
        if (reply->type == REDIS_REPLY_STRING && reply->str) {
            int64_t ts = (int64_t)strtoll(reply->str, NULL, 10);
            if (ts > 0) {
                for (int i = 0; i < LLZ_SERVICE_COUNT; i++) {
                    if (g_state.services[i].lastUpdated < ts) {
                        g_state.services[i].lastChecked = ts;
                    }
                }
            }
        }
        freeReplyObject(reply);
    }
}

// ============================================================================
// Public API - Initialization
// ============================================================================

bool LlzConnectionsInit(const LlzConnectionsConfig *config)
{
    if (g_initialized) {
        return true;
    }

    // Apply config with defaults
    g_config.autoCheckIntervalSeconds = LLZ_CONNECTION_DEFAULT_INTERVAL;
    g_config.checkOnInit = true;

    if (config) {
        if (config->autoCheckIntervalSeconds >= 0) {
            g_config.autoCheckIntervalSeconds = config->autoCheckIntervalSeconds;
        }
        g_config.checkOnInit = config->checkOnInit;
    }

    llz_conn_init_state();

    if (!llz_conn_connect()) {
        // Connection failed but we can retry later
        g_initialized = true;
        return true;
    }

    g_initialized = true;
    g_autoCheckEnabled = (g_config.autoCheckIntervalSeconds > 0);
    g_timeSinceLastCheck = 0.0f;

    // Initial status read from Redis (in case there's cached data)
    llz_conn_read_status_from_redis();

    // Request fresh status if configured
    if (g_config.checkOnInit) {
        LlzConnectionsRefresh();
    }

    return true;
}

void LlzConnectionsShutdown(void)
{
    llz_conn_disconnect();
    memset(&g_state, 0, sizeof(g_state));
    g_initialized = false;
}

// ============================================================================
// Public API - Status Retrieval
// ============================================================================

bool LlzConnectionsGetState(LlzConnectionsState *outState)
{
    if (!outState || !g_initialized) return false;

    // Read latest from Redis before returning
    llz_conn_read_status_from_redis();

    memcpy(outState, &g_state, sizeof(LlzConnectionsState));
    return true;
}

bool LlzConnectionsGetServiceStatus(LlzServiceType service, LlzServiceStatus *outStatus)
{
    if (!outStatus || !g_initialized) return false;
    if (service < 0 || service >= LLZ_SERVICE_COUNT) return false;

    // Read latest from Redis before returning
    llz_conn_read_status_from_redis();

    memcpy(outStatus, &g_state.services[service], sizeof(LlzServiceStatus));
    return true;
}

bool LlzConnectionsIsConnected(LlzServiceType service)
{
    if (!g_initialized) return false;
    if (service < 0 || service >= LLZ_SERVICE_COUNT) return false;

    llz_conn_read_status_from_redis();
    return g_state.services[service].state == LLZ_CONN_STATUS_CONNECTED;
}

LlzConnectionState LlzConnectionsGetServiceState(LlzServiceType service)
{
    if (!g_initialized) return LLZ_CONN_STATUS_UNKNOWN;
    if (service < 0 || service >= LLZ_SERVICE_COUNT) return LLZ_CONN_STATUS_UNKNOWN;

    llz_conn_read_status_from_redis();
    return g_state.services[service].state;
}

// ============================================================================
// Public API - Refresh Control
// ============================================================================

bool LlzConnectionsRefresh(void)
{
    if (!g_initialized) return false;
    return llz_conn_send_status_request(NULL);
}

bool LlzConnectionsRefreshService(LlzServiceType service)
{
    if (!g_initialized) return false;
    if (service < 0 || service >= LLZ_SERVICE_COUNT) return false;

    return llz_conn_send_status_request(g_serviceNames[service]);
}

// ============================================================================
// Public API - Auto-Check Control
// ============================================================================

void LlzConnectionsUpdate(float deltaTime)
{
    if (!g_initialized) return;

    // Always try to read latest status from Redis
    llz_conn_read_status_from_redis();

    // Handle auto-check
    if (!g_autoCheckEnabled || g_config.autoCheckIntervalSeconds <= 0) {
        return;
    }

    g_timeSinceLastCheck += deltaTime;

    if (g_timeSinceLastCheck >= (float)g_config.autoCheckIntervalSeconds) {
        g_timeSinceLastCheck = 0.0f;
        LlzConnectionsRefresh();
    }
}

void LlzConnectionsSetAutoCheckInterval(int intervalSeconds)
{
    g_config.autoCheckIntervalSeconds = intervalSeconds;
    g_autoCheckEnabled = (intervalSeconds > 0);
}

int LlzConnectionsGetAutoCheckInterval(void)
{
    return g_config.autoCheckIntervalSeconds;
}

void LlzConnectionsSetAutoCheckEnabled(bool enabled)
{
    g_autoCheckEnabled = enabled;
    if (enabled && g_config.autoCheckIntervalSeconds <= 0) {
        g_config.autoCheckIntervalSeconds = LLZ_CONNECTION_DEFAULT_INTERVAL;
    }
}

bool LlzConnectionsIsAutoCheckEnabled(void)
{
    return g_autoCheckEnabled;
}

// ============================================================================
// Public API - Utility Functions
// ============================================================================

const char *LlzConnectionsGetServiceName(LlzServiceType service)
{
    if (service < 0 || service >= LLZ_SERVICE_COUNT) {
        return "unknown";
    }
    return g_serviceNames[service];
}

const char *LlzConnectionsGetStateString(LlzConnectionState state)
{
    if (state < 0 || state > LLZ_CONN_STATUS_CHECKING) {
        return "Unknown";
    }
    return g_stateStrings[state];
}

bool LlzConnectionsIsRefreshing(void)
{
    if (!g_initialized) return false;
    return g_state.refreshInProgress;
}

int64_t LlzConnectionsGetTimeSinceRefresh(void)
{
    if (!g_initialized) return -1;
    if (g_state.lastRefresh <= 0) return -1;

    return (int64_t)time(NULL) - g_state.lastRefresh;
}
