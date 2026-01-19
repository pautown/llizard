#include "host_input.h"
#include <string.h>

#ifdef PLATFORM_DRM
#include "carthing_input.h"
#endif

bool hostSimulatedMousePressed = false;
bool hostSimulatedMouseJustPressed = false;
bool hostSimulatedMouseJustReleased = false;
Vector2 hostSimulatedMousePos = {0, 0};
float hostSimulatedScrollWheel = 0.0f;

static HostInputState g_lastState;

void HostInputInit(void) {
    #ifdef PLATFORM_DRM
    CTInputInit();
    #endif
}

void HostInputUpdate(HostInputState *state) {
    if (!state) return;
    memset(state, 0, sizeof(HostInputState));

#ifdef PLATFORM_DRM
    // Process CarThing input events
    CTInputEvent event;
    hostSimulatedMouseJustPressed = false;
    hostSimulatedMouseJustReleased = false;
    hostSimulatedScrollWheel = 0.0f;

    while (CTInputPollEvent(&event)) {
        switch (event.type) {
            case CT_EVENT_BUTTON_PRESS:
                if (event.button.button == CT_BUTTON_BACK) state->backPressed = true;
                else if (event.button.button == CT_BUTTON_SELECT) {
                    state->selectPressed = true;
                    state->playPausePressed = true;
                }
                else if (event.button.button == CT_BUTTON_1) {
                    state->button1Pressed = true;
                    state->upPressed = true;
                }
                else if (event.button.button == CT_BUTTON_2) {
                    state->button2Pressed = true;
                    state->downPressed = true;
                }
                else if (event.button.button == CT_BUTTON_3) {
                    state->button3Pressed = true;
                    state->displayModeNext = true;
                }
                else if (event.button.button == CT_BUTTON_4) {
                    state->button4Pressed = true;
                    state->styleCyclePressed = true;
                }
                else if (event.button.button == CT_BUTTON_SCREENSHOT) {
                    state->screenshotPressed = true;
                    state->button5Pressed = true;
                }
                break;
            case CT_EVENT_SCROLL:
                state->scrollDelta = (float)event.scroll.delta;
                hostSimulatedScrollWheel = (float)event.scroll.delta;
                break;
            case CT_EVENT_TOUCH_PRESS:
                hostSimulatedMousePressed = true;
                hostSimulatedMouseJustPressed = true;
                hostSimulatedMousePos = (Vector2){(float)event.touch.x, (float)event.touch.y};
                break;
            case CT_EVENT_TOUCH_MOVE:
                hostSimulatedMousePos = (Vector2){(float)event.touch.x, (float)event.touch.y};
                break;
            case CT_EVENT_TOUCH_RELEASE:
                hostSimulatedMousePressed = false;
                hostSimulatedMouseJustReleased = true;
                break;
            default:
                break;
        }
    }

    state->mousePos = hostSimulatedMousePos;
    state->mousePressed = hostSimulatedMousePressed;
    state->mouseJustPressed = hostSimulatedMouseJustPressed;
    state->mouseJustReleased = hostSimulatedMouseJustReleased;
#else
    // Desktop input
    state->backPressed = IsKeyPressed(KEY_ESCAPE);
    if (IsKeyPressed(KEY_ENTER)) {
        state->selectPressed = true;
        state->playPausePressed = true;
    }
    if (IsKeyPressed(KEY_ONE)) {
        state->button1Pressed = true;
        state->upPressed = true;
    }
    if (IsKeyPressed(KEY_TWO)) {
        state->button2Pressed = true;
        state->downPressed = true;
    }
    if (IsKeyPressed(KEY_THREE)) {
        state->button3Pressed = true;
        state->displayModeNext = true;
    }
    if (IsKeyPressed(KEY_FOUR)) {
        state->button4Pressed = true;
        state->styleCyclePressed = true;
    }
    if (IsKeyPressed(KEY_FIVE)) {
        state->button5Pressed = true;
        state->screenshotPressed = true;
    }
    state->screenshotPressed = state->screenshotPressed || IsKeyPressed(KEY_F1);
    state->displayModeNext = state->displayModeNext || IsKeyPressed(KEY_M);
    state->styleCyclePressed = state->styleCyclePressed || IsKeyPressed(KEY_B);
    state->scrollDelta = GetMouseWheelMove();
    state->mousePos = GetMousePosition();
    state->mousePressed = IsMouseButtonDown(MOUSE_LEFT_BUTTON);
    state->mouseJustPressed = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    state->mouseJustReleased = IsMouseButtonReleased(MOUSE_LEFT_BUTTON);
#endif

    g_lastState = *state;
}

void HostInputShutdown(void) {
#ifdef PLATFORM_DRM
    CTInputClose();
#endif
}

const HostInputState *HostInputGetState(void)
{
    return &g_lastState;
}
