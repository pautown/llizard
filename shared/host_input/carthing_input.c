#include "carthing_input.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/input.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <math.h>

// Device paths
#define DEVICE_BUTTONS   "/dev/input/event0"
#define DEVICE_ROTARY    "/dev/input/event1"
#define DEVICE_TOUCH     "/dev/input/event3"
#define DEVICE_TOUCH_FB  "/dev/input/event2"

// Touch debounce time in microseconds
#define TOUCH_DEBOUNCE_US 2000

// Multitouch event codes (in case not defined in linux/input.h)
#ifndef ABS_MT_POSITION_X
#define ABS_MT_POSITION_X  0x35  // 53
#endif
#ifndef ABS_MT_POSITION_Y
#define ABS_MT_POSITION_Y  0x36  // 54
#endif
#ifndef BTN_TOOL_FINGER
#define BTN_TOOL_FINGER    0x145 // 325
#endif

// Input device state
typedef struct {
    int fd_buttons;
    int fd_rotary;
    int fd_touch;
    char touch_device_path[32];

    // Button states
    bool button_states[256];

    // Touch state
    bool touch_active;
    int touch_x;
    int touch_y;
    int raw_x;  // Raw coordinates before transformation
    int raw_y;
    int touch_min_x;
    int touch_max_x;
    int touch_min_y;
    int touch_max_y;
    struct timeval last_touch_time;

    // Event queue
    CTInputEvent event_queue[32];
    int queue_head;
    int queue_tail;
} CTInputState;

static CTInputState g_input_state = {0};

#define TEST_BIT(bit, array) (((array)[(bit) / (sizeof(long) * 8)] >> ((bit) % (sizeof(long) * 8))) & 0x1)

static bool is_touch_device(int fd) {
    unsigned long ev_bits[(EV_MAX + (sizeof(long) * 8) - 1) / (sizeof(long) * 8)] = {0};
    unsigned long abs_bits[(ABS_MAX + (sizeof(long) * 8) - 1) / (sizeof(long) * 8)] = {0};

    if (ioctl(fd, EVIOCGBIT(0, sizeof(ev_bits)), ev_bits) < 0) {
        return false;
    }

    if (!TEST_BIT(EV_ABS, ev_bits)) {
        return false;
    }

    if (ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(abs_bits)), abs_bits) < 0) {
        return false;
    }

    bool hasX = TEST_BIT(ABS_X, abs_bits) || TEST_BIT(ABS_MT_POSITION_X, abs_bits);
    bool hasY = TEST_BIT(ABS_Y, abs_bits) || TEST_BIT(ABS_MT_POSITION_Y, abs_bits);

    return hasX && hasY;
}

static void configure_touch_range(int fd) {
    struct input_absinfo absinfo = {0};
    bool range_valid = false;

    if (ioctl(fd, EVIOCGABS(ABS_X), &absinfo) == 0 && absinfo.maximum - absinfo.minimum > 10) {
        g_input_state.touch_min_x = absinfo.minimum;
        g_input_state.touch_max_x = absinfo.maximum;
        range_valid = true;
    } else if (ioctl(fd, EVIOCGABS(ABS_MT_POSITION_X), &absinfo) == 0 && absinfo.maximum - absinfo.minimum > 10) {
        g_input_state.touch_min_x = absinfo.minimum;
        g_input_state.touch_max_x = absinfo.maximum;
        range_valid = true;
    }
    if (!range_valid) {
        g_input_state.touch_min_x = 0;
        g_input_state.touch_max_x = 4095;
    }

    range_valid = false;
    if (ioctl(fd, EVIOCGABS(ABS_Y), &absinfo) == 0 && absinfo.maximum - absinfo.minimum > 10) {
        g_input_state.touch_min_y = absinfo.minimum;
        g_input_state.touch_max_y = absinfo.maximum;
        range_valid = true;
    } else if (ioctl(fd, EVIOCGABS(ABS_MT_POSITION_Y), &absinfo) == 0 && absinfo.maximum - absinfo.minimum > 10) {
        g_input_state.touch_min_y = absinfo.minimum;
        g_input_state.touch_max_y = absinfo.maximum;
        range_valid = true;
    }
    if (!range_valid) {
        g_input_state.touch_min_y = 0;
        g_input_state.touch_max_y = 4095;
    }

    if (g_input_state.touch_max_x <= g_input_state.touch_min_x) {
        g_input_state.touch_max_x = g_input_state.touch_min_x + 1;
    }
    if (g_input_state.touch_max_y <= g_input_state.touch_min_y) {
        g_input_state.touch_max_y = g_input_state.touch_min_y + 1;
    }

    printf("[CTInput] Touch range X:[%d,%d] Y:[%d,%d]\n",
           g_input_state.touch_min_x, g_input_state.touch_max_x,
           g_input_state.touch_min_y, g_input_state.touch_max_y);
}

static int open_touch_device(void) {
    const char *candidates[] = {
        DEVICE_TOUCH,
        DEVICE_TOUCH_FB,
        "/dev/input/event4",
        "/dev/input/event5"
    };

    for (size_t i = 0; i < sizeof(candidates)/sizeof(candidates[0]); ++i) {
        int fd = open(candidates[i], O_RDONLY | O_NONBLOCK);
        if (fd < 0) continue;

        if (is_touch_device(fd)) {
            strncpy(g_input_state.touch_device_path, candidates[i], sizeof(g_input_state.touch_device_path) - 1);
            g_input_state.touch_device_path[sizeof(g_input_state.touch_device_path) - 1] = '\0';
            configure_touch_range(fd);
            return fd;
        }

        close(fd);
    }

    // Fallback: try scanning event nodes if the above list failed
    for (int idx = 0; idx < 10; ++idx) {
        char path[32];
        snprintf(path, sizeof(path), "/dev/input/event%d", idx);
        int fd = open(path, O_RDONLY | O_NONBLOCK);
        if (fd < 0) continue;
        if (is_touch_device(fd)) {
            strncpy(g_input_state.touch_device_path, path, sizeof(g_input_state.touch_device_path) - 1);
            g_input_state.touch_device_path[sizeof(g_input_state.touch_device_path) - 1] = '\0';
            configure_touch_range(fd);
            return fd;
        }
        close(fd);
    }

    return -1;
}

// Helper: Check if debounce time has elapsed
static bool is_debounce_elapsed(void) {
    struct timeval now;
    gettimeofday(&now, NULL);

    long elapsed_us = (now.tv_sec - g_input_state.last_touch_time.tv_sec) * 1000000 +
                      (now.tv_usec - g_input_state.last_touch_time.tv_usec);

    bool result = elapsed_us >= TOUCH_DEBOUNCE_US;
    printf("[CTInput][DEBOUNCE] now=(%ld,%ld) last=(%ld,%ld) elapsed_us=%ld threshold=%d result=%d\n",
           now.tv_sec, now.tv_usec,
           g_input_state.last_touch_time.tv_sec, g_input_state.last_touch_time.tv_usec,
           elapsed_us, TOUCH_DEBOUNCE_US, result);
    return result;
}

static float normalize_raw(int value, int min, int max)
{
    if (max <= min) return 0.0f;
    float norm = (float)(value - min) / (float)(max - min);
    if (norm < 0.0f) norm = 0.0f;
    if (norm > 1.0f) norm = 1.0f;
    return norm;
}

// Helper: Transform touch coordinates from portrait (480×800) to landscape (800×480)
static void transform_touch_coords(int raw_x, int raw_y, int *out_x, int *out_y) {
    const int portraitWidth = 480;
    const int portraitHeight = 800;

    float normX = normalize_raw(raw_x, g_input_state.touch_min_x, g_input_state.touch_max_x);
    float normY = normalize_raw(raw_y, g_input_state.touch_min_y, g_input_state.touch_max_y);

    int portraitX = (int)lrintf(normX * (portraitWidth - 1));
    int portraitY = (int)lrintf(normY * (portraitHeight - 1));

    *out_x = portraitY;
    *out_y = (portraitWidth - 1) - portraitX;

    if (*out_x < 0) *out_x = 0;
    if (*out_x > 799) *out_x = 799;
    if (*out_y < 0) *out_y = 0;
    if (*out_y > 479) *out_y = 479;
}

// Helper: Queue an event
static void queue_event(CTInputEvent *event) {
    int next_tail = (g_input_state.queue_tail + 1) % 32;
    if (next_tail != g_input_state.queue_head) {
        g_input_state.event_queue[g_input_state.queue_tail] = *event;
        g_input_state.queue_tail = next_tail;
    }
}

static CTButton normalize_button_code(int code)
{
    switch (code) {
        case 1: return CT_BUTTON_BACK;
        case 28: return CT_BUTTON_SELECT;
        case 2: return CT_BUTTON_1;
        case 3: return CT_BUTTON_2;
        case 4: return CT_BUTTON_3;
        case 5: return CT_BUTTON_4;
        case 50: return CT_BUTTON_SCREENSHOT;
        default: return CT_BUTTON_NONE;
    }
}

// Helper: Process button event
static void process_button_event(struct input_event *ev) {
    if (ev->type != EV_KEY) return;

    CTButton button = normalize_button_code(ev->code);
    if (button == CT_BUTTON_NONE) return;
    bool is_press = (ev->value == 1);
    bool is_release = (ev->value == 0);

    // Update button state
    if (is_press || is_release) {
        g_input_state.button_states[ev->code] = is_press;

        printf("[CTInput] Button %s: code=%d (mapped=%d)\n", is_press ? "PRESS" : "RELEASE", ev->code, button);

        CTInputEvent event = {0};
        event.type = is_press ? CT_EVENT_BUTTON_PRESS : CT_EVENT_BUTTON_RELEASE;
        event.button.button = button;
        queue_event(&event);
    }
}

// Helper: Process rotary encoder event
static void process_rotary_event(struct input_event *ev) {
    if (ev->type != EV_REL || ev->code != REL_HWHEEL) return;

    printf("[CTInput] Rotary SCROLL: delta=%d\n", ev->value);

    CTInputEvent event = {0};
    event.type = CT_EVENT_SCROLL;
    event.scroll.delta = ev->value;
    queue_event(&event);
}

// Helper: Process touch event
static void process_touch_event(struct input_event *ev) {
    static bool pending_coords = false;

    printf("[CTInput][RAW] type=%d code=%d value=%d\n", ev->type, ev->code, ev->value);

    // Always update coordinates first (they come before press/release events)
    if (ev->type == EV_ABS) {
        // Handle both single-touch (ABS_X/ABS_Y) and multitouch (ABS_MT_POSITION_X/Y)
        if (ev->code == ABS_X || ev->code == ABS_MT_POSITION_X) {
            g_input_state.raw_x = ev->value;
            pending_coords = true;
        } else if (ev->code == ABS_Y || ev->code == ABS_MT_POSITION_Y) {
            g_input_state.raw_y = ev->value;
            pending_coords = true;
        }
    }

    // Then handle press/release
    if (ev->type == EV_KEY && (ev->code == BTN_TOUCH || ev->code == BTN_TOOL_FINGER || ev->code == 330)) {
        bool is_press = (ev->value == 1);
        printf("[CTInput][DEBUG] Touch key event: code=%d value=%d active=%d debounce=%d raw=(%d,%d)\n",
               ev->code, ev->value, g_input_state.touch_active, is_debounce_elapsed(),
               g_input_state.raw_x, g_input_state.raw_y);

        // Temporarily disable debounce check to diagnose touch issues
        // if (is_press && !g_input_state.touch_active && is_debounce_elapsed()) {
        if (is_press && !g_input_state.touch_active) {
            // Touch press
            g_input_state.touch_active = true;
            gettimeofday(&g_input_state.last_touch_time, NULL);

            transform_touch_coords(g_input_state.raw_x, g_input_state.raw_y,
                                 &g_input_state.touch_x, &g_input_state.touch_y);

            printf("[CTInput] Touch PRESS: raw(%d,%d) -> landscape(%d,%d)\n",
                   g_input_state.raw_x, g_input_state.raw_y,
                   g_input_state.touch_x, g_input_state.touch_y);

            CTInputEvent event = {0};
            event.type = CT_EVENT_TOUCH_PRESS;
            event.touch.x = g_input_state.touch_x;
            event.touch.y = g_input_state.touch_y;
            queue_event(&event);
        } else if (!is_press && g_input_state.touch_active) {
            // Touch release
            g_input_state.touch_active = false;
            gettimeofday(&g_input_state.last_touch_time, NULL);

            printf("[CTInput] Touch RELEASE: (%d,%d)\n",
                   g_input_state.touch_x, g_input_state.touch_y);

            CTInputEvent event = {0};
            event.type = CT_EVENT_TOUCH_RELEASE;
            event.touch.x = g_input_state.touch_x;
            event.touch.y = g_input_state.touch_y;
            queue_event(&event);
        }
    } else if (ev->type == EV_SYN && ev->code == SYN_REPORT && pending_coords) {
        // Coordinate update complete (move event)
        if (g_input_state.touch_active) {
            int new_x, new_y;
            transform_touch_coords(g_input_state.raw_x, g_input_state.raw_y, &new_x, &new_y);

            if (new_x != g_input_state.touch_x || new_y != g_input_state.touch_y) {
                g_input_state.touch_x = new_x;
                g_input_state.touch_y = new_y;

                CTInputEvent event = {0};
                event.type = CT_EVENT_TOUCH_MOVE;
                event.touch.x = g_input_state.touch_x;
                event.touch.y = g_input_state.touch_y;
                queue_event(&event);
            }
        }
        pending_coords = false;
    }
}

// Helper: Poll a single device
static void poll_device(int fd, void (*handler)(struct input_event *)) {
    if (fd < 0) return;

    struct input_event ev;
    while (read(fd, &ev, sizeof(ev)) == sizeof(ev)) {
        handler(&ev);
    }
}

bool CTInputInit(void) {
    memset(&g_input_state, 0, sizeof(g_input_state));
    g_input_state.fd_buttons = -1;
    g_input_state.fd_rotary = -1;
    g_input_state.fd_touch = -1;
    g_input_state.touch_min_x = 0;
    g_input_state.touch_max_x = 480;
    g_input_state.touch_min_y = 0;
    g_input_state.touch_max_y = 800;

    // Open button device
    g_input_state.fd_buttons = open(DEVICE_BUTTONS, O_RDONLY | O_NONBLOCK);
    if (g_input_state.fd_buttons < 0) {
        fprintf(stderr, "Warning: Failed to open buttons device %s: %s\n",
                DEVICE_BUTTONS, strerror(errno));
    }

    // Open rotary encoder device
    g_input_state.fd_rotary = open(DEVICE_ROTARY, O_RDONLY | O_NONBLOCK);
    if (g_input_state.fd_rotary < 0) {
        fprintf(stderr, "Warning: Failed to open rotary device %s: %s\n",
                DEVICE_ROTARY, strerror(errno));
    }

    // Open touch device (with fallback)
    g_input_state.fd_touch = open_touch_device();
    if (g_input_state.fd_touch < 0) {
        fprintf(stderr, "Warning: Failed to open touch device: %s\n", strerror(errno));
    }

    // At least one device should be available
    if (g_input_state.fd_buttons < 0 && g_input_state.fd_rotary < 0 && g_input_state.fd_touch < 0) {
        fprintf(stderr, "Error: No input devices available\n");
        return false;
    }

    printf("CarThing Input: Initialized (buttons:%s rotary:%s touch:%s)\n",
           g_input_state.fd_buttons >= 0 ? "OK" : "FAIL",
           g_input_state.fd_rotary >= 0 ? "OK" : "FAIL",
           g_input_state.fd_touch >= 0 ? "OK" : "FAIL");
    if (g_input_state.fd_touch >= 0) {
        printf("[CTInput] Touch device: %s\n", g_input_state.touch_device_path[0] ? g_input_state.touch_device_path : "<unknown>");
    }

    return true;
}

void CTInputClose(void) {
    if (g_input_state.fd_buttons >= 0) {
        close(g_input_state.fd_buttons);
        g_input_state.fd_buttons = -1;
    }
    if (g_input_state.fd_rotary >= 0) {
        close(g_input_state.fd_rotary);
        g_input_state.fd_rotary = -1;
    }
    if (g_input_state.fd_touch >= 0) {
        close(g_input_state.fd_touch);
        g_input_state.fd_touch = -1;
    }
}

bool CTInputPollEvent(CTInputEvent *event) {
    // Poll all devices for new events
    poll_device(g_input_state.fd_buttons, process_button_event);
    poll_device(g_input_state.fd_rotary, process_rotary_event);
    poll_device(g_input_state.fd_touch, process_touch_event);

    // Dequeue next event if available
    if (g_input_state.queue_head != g_input_state.queue_tail) {
        *event = g_input_state.event_queue[g_input_state.queue_head];
        g_input_state.queue_head = (g_input_state.queue_head + 1) % 32;
        return true;
    }

    return false;
}

bool CTInputIsButtonDown(CTButton button) {
    if (button < 0 || button >= 256) return false;
    return g_input_state.button_states[button];
}

bool CTInputGetTouchPosition(int *x, int *y) {
    if (!g_input_state.touch_active) return false;

    if (x) *x = g_input_state.touch_x;
    if (y) *y = g_input_state.touch_y;
    return true;
}
