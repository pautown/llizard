#ifndef LLZ_SDK_CONNECTIONS_H
#define LLZ_SDK_CONNECTIONS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Service Connection Status API
// ============================================================================
//
// This module manages connection status checking for external services
// (Spotify, etc.) via the Android companion app. It communicates through
// the BLE bridge (golang_ble_client) using Redis as the message bus.
//
// Data Flow:
//   SDK -> Redis (connection_status_q) -> golang_ble_client -> BLE -> Android
//   Android -> BLE -> golang_ble_client -> Redis (connections:*) -> SDK
//

#define LLZ_CONNECTION_SERVICE_NAME_MAX 32
#define LLZ_CONNECTION_ERROR_MAX 128
#define LLZ_CONNECTION_MAX_SERVICES 16

// Default auto-check interval in seconds (3 minutes)
#define LLZ_CONNECTION_DEFAULT_INTERVAL 180

// ============================================================================
// Service Types
// ============================================================================

typedef enum {
    LLZ_SERVICE_SPOTIFY = 0,
    LLZ_SERVICE_COUNT
} LlzServiceType;

// ============================================================================
// Connection Status
// ============================================================================

typedef enum {
    LLZ_CONN_STATUS_UNKNOWN = 0,    // Never checked or no response
    LLZ_CONN_STATUS_CONNECTED,       // Service is authenticated and connected
    LLZ_CONN_STATUS_DISCONNECTED,    // Service is not connected/authenticated
    LLZ_CONN_STATUS_ERROR,           // Error checking status
    LLZ_CONN_STATUS_CHECKING         // Currently checking (request in flight)
} LlzConnectionState;

// Status for a single service
typedef struct {
    LlzServiceType service;
    LlzConnectionState state;
    char serviceName[LLZ_CONNECTION_SERVICE_NAME_MAX];
    char error[LLZ_CONNECTION_ERROR_MAX];      // Error message if state is ERROR
    int64_t lastChecked;                        // Unix timestamp of last check
    int64_t lastUpdated;                        // Unix timestamp of last status update
} LlzServiceStatus;

// Combined status for all services
typedef struct {
    LlzServiceStatus services[LLZ_CONNECTION_MAX_SERVICES];
    int serviceCount;
    int64_t lastRefresh;                        // When we last requested a refresh
    bool refreshInProgress;                     // True if waiting for response
} LlzConnectionsState;

// ============================================================================
// Configuration
// ============================================================================

typedef struct {
    int autoCheckIntervalSeconds;               // 0 to disable auto-check
    bool checkOnInit;                           // Request status on init
} LlzConnectionsConfig;

// ============================================================================
// Initialization
// ============================================================================

// Initialize the connections module
// config: Optional configuration (pass NULL for defaults)
// Returns true if initialization succeeded
bool LlzConnectionsInit(const LlzConnectionsConfig *config);

// Shutdown the connections module
void LlzConnectionsShutdown(void);

// ============================================================================
// Status Retrieval
// ============================================================================

// Get the current status for all services
// outState: Pointer to receive the connection state
// Returns true if state was retrieved successfully
bool LlzConnectionsGetState(LlzConnectionsState *outState);

// Get the status for a specific service
// service: The service to query
// outStatus: Pointer to receive the service status
// Returns true if status was retrieved successfully
bool LlzConnectionsGetServiceStatus(LlzServiceType service, LlzServiceStatus *outStatus);

// Check if a specific service is connected
// service: The service to check
// Returns true if the service is in CONNECTED state
bool LlzConnectionsIsConnected(LlzServiceType service);

// Get the connection state for a specific service
// service: The service to check
// Returns the connection state
LlzConnectionState LlzConnectionsGetServiceState(LlzServiceType service);

// ============================================================================
// Refresh Control
// ============================================================================

// Request a manual refresh of connection status
// This sends a request to the Android app via BLE
// Returns true if the request was queued successfully
bool LlzConnectionsRefresh(void);

// Request status for a specific service only
// service: The service to check
// Returns true if the request was queued successfully
bool LlzConnectionsRefreshService(LlzServiceType service);

// ============================================================================
// Auto-Check Control
// ============================================================================

// Update the connections state (call this periodically from main loop)
// This handles auto-checking and reading responses from Redis
// deltaTime: Time since last update in seconds
void LlzConnectionsUpdate(float deltaTime);

// Set the auto-check interval
// intervalSeconds: Seconds between checks (0 to disable)
void LlzConnectionsSetAutoCheckInterval(int intervalSeconds);

// Get the current auto-check interval
int LlzConnectionsGetAutoCheckInterval(void);

// Enable/disable auto-checking
void LlzConnectionsSetAutoCheckEnabled(bool enabled);
bool LlzConnectionsIsAutoCheckEnabled(void);

// ============================================================================
// Utility Functions
// ============================================================================

// Get human-readable name for a service type
const char *LlzConnectionsGetServiceName(LlzServiceType service);

// Get human-readable state description
const char *LlzConnectionsGetStateString(LlzConnectionState state);

// Check if any service is currently in CHECKING state
bool LlzConnectionsIsRefreshing(void);

// Get time since last successful refresh (in seconds)
// Returns -1 if never refreshed
int64_t LlzConnectionsGetTimeSinceRefresh(void);

#ifdef __cplusplus
}
#endif

#endif // LLZ_SDK_CONNECTIONS_H
