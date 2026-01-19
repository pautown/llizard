#include "llz_sdk_input.h"
#include "llz_sdk_config.h"

#include <math.h>
#include <string.h>
#include <stdio.h>

#ifdef PLATFORM_DRM
#include "carthing_input.h"
#endif

bool llzSimulatedMousePressed = false;
bool llzSimulatedMouseJustPressed = false;
bool llzSimulatedMouseJustReleased = false;
Vector2 llzSimulatedMousePos = {0, 0};
float llzSimulatedScrollWheel = 0.0f;

static LlzInputState g_state;
static bool g_touchActive = false;
static bool g_holdReported = false;
static double g_touchStartTime = 0.0;
static Vector2 g_touchStartPos = {0};
static double g_lastTapTime = 0.0;
static Vector2 g_lastTapPos = {0};
static Vector2 g_dragStartPos = {0};
static Vector2 g_prevDragPos = {0};

// Button hold tracking (buttons 1-6)
static bool g_buttonDown[6] = {false};
static double g_buttonDownStartTime[6] = {0};
static bool g_buttonHoldReported[6] = {false};
static const float BUTTON_HOLD_THRESHOLD = 0.5f;  // seconds

// Back button tracking
static bool g_backButtonDown = false;

// Select button tracking (with hold detection)
static bool g_selectButtonDown = false;
static double g_selectButtonDownStartTime = 0.0;
static bool g_selectHoldReported = false;

// Helper to handle button press event
static void HandleButtonPress(int buttonIndex) {
    if (buttonIndex < 0 || buttonIndex >= 6) return;
    if (!g_buttonDown[buttonIndex]) {
        g_buttonDown[buttonIndex] = true;
        g_buttonDownStartTime[buttonIndex] = GetTime();
        g_buttonHoldReported[buttonIndex] = false;
        printf("[SDK] Button %d pressed (down)\n", buttonIndex + 1);
    }
}

// Helper to handle button release event
static void HandleButtonRelease(int buttonIndex, LlzInputState *state) {
    if (buttonIndex < 0 || buttonIndex >= 6) return;
    if (g_buttonDown[buttonIndex]) {
        double holdTime = GetTime() - g_buttonDownStartTime[buttonIndex];
        printf("[SDK] Button %d released after %.3fs (%s)\n",
               buttonIndex + 1, holdTime,
               holdTime < BUTTON_HOLD_THRESHOLD ? "CLICK" : "HOLD");
        // If released quickly (before hold threshold), it's a click/press
        if (holdTime < BUTTON_HOLD_THRESHOLD) {
            switch (buttonIndex) {
                case 0: state->button1Pressed = true; break;
                case 1: state->button2Pressed = true; break;
                case 2: state->button3Pressed = true; break;
                case 3: state->button4Pressed = true; break;
                case 4: state->button5Pressed = true; break;
                case 5:
                    state->button6Pressed = true;
                    // Button 6 (screenshot button) toggles screen brightness on/off
                    LlzConfigToggleBrightness();
                    break;
            }
        }
        g_buttonDown[buttonIndex] = false;
        g_buttonDownStartTime[buttonIndex] = 0;
        g_buttonHoldReported[buttonIndex] = false;
    }
}

// Update button states based on current down state and time
static void UpdateButtonStates(LlzInputState *state, float deltaTime) {
    double currentTime = GetTime();
    for (int i = 0; i < 6; i++) {
        bool isDown = g_buttonDown[i];
        float holdTime = isDown ? (float)(currentTime - g_buttonDownStartTime[i]) : 0.0f;
        bool holdTriggered = false;

        if (isDown && holdTime >= BUTTON_HOLD_THRESHOLD && !g_buttonHoldReported[i]) {
            holdTriggered = true;
            g_buttonHoldReported[i] = true;
            printf("[SDK] Button %d HOLD triggered at %.3fs\n", i + 1, holdTime);
        }

        switch (i) {
            case 0:
                state->button1Down = isDown;
                state->button1Hold = holdTriggered;
                state->button1HoldTime = holdTime;
                break;
            case 1:
                state->button2Down = isDown;
                state->button2Hold = holdTriggered;
                state->button2HoldTime = holdTime;
                break;
            case 2:
                state->button3Down = isDown;
                state->button3Hold = holdTriggered;
                state->button3HoldTime = holdTime;
                break;
            case 3:
                state->button4Down = isDown;
                state->button4Hold = holdTriggered;
                state->button4HoldTime = holdTime;
                break;
            case 4:
                state->button5Down = isDown;
                state->button5Hold = holdTriggered;
                state->button5HoldTime = holdTime;
                break;
            case 5:
                state->button6Down = isDown;
                state->button6Hold = holdTriggered;
                state->button6HoldTime = holdTime;
                break;
        }
    }
}

static void UpdateSelectButtonState(LlzInputState *state) {
    double currentTime = GetTime();
    float holdTime = g_selectButtonDown ? (float)(currentTime - g_selectButtonDownStartTime) : 0.0f;
    bool holdTriggered = false;

    if (g_selectButtonDown && holdTime >= BUTTON_HOLD_THRESHOLD && !g_selectHoldReported) {
        holdTriggered = true;
        g_selectHoldReported = true;
    }

    state->selectDown = g_selectButtonDown;
    state->selectHold = holdTriggered;
    state->selectHoldTime = holdTime;
}

static void ProcessGestureRelease(LlzInputState *state, const Vector2 *endPos)
{
    double elapsed = GetTime() - g_touchStartTime;
    Vector2 delta = {endPos->x - g_touchStartPos.x, endPos->y - g_touchStartPos.y};
    float dist = sqrtf(delta.x * delta.x + delta.y * delta.y);
    const float tapThreshold = 30.0f;
    const float swipeThreshold = 80.0f;

    if (elapsed < 0.3 && dist < tapThreshold) {
        state->tap = true;
        state->tapPosition = *endPos;
        if (g_touchStartTime - g_lastTapTime < 0.35f &&
            fabsf(endPos->x - g_lastTapPos.x) < tapThreshold &&
            fabsf(endPos->y - g_lastTapPos.y) < tapThreshold) {
            state->doubleTap = true;
        }
        g_lastTapTime = g_touchStartTime;
        g_lastTapPos = *endPos;
    } else if (dist >= swipeThreshold) {
        state->swipeDelta = delta;
        state->swipeStart = g_touchStartPos;
        state->swipeEnd = *endPos;
        if (fabsf(delta.x) > fabsf(delta.y)) {
            if (delta.x > 0) state->swipeRight = true;
            else state->swipeLeft = true;
        } else {
            if (delta.y > 0) state->swipeDown = true;
            else state->swipeUp = true;
        }
    }
}

static void MaybeReportHold(LlzInputState *state, const Vector2 *pos)
{
    if (g_holdReported || !g_touchActive) return;
    if (GetTime() - g_touchStartTime > 0.7f) {
        state->hold = true;
        state->holdPosition = *pos;
        g_holdReported = true;
    }
}

void LlzInputInit(void)
{
#ifdef PLATFORM_DRM
    CTInputInit();
#endif
}

void LlzInputUpdate(LlzInputState *state)
{
    if (!state) state = &g_state;
    memset(state, 0, sizeof(*state));

#ifdef PLATFORM_DRM
    CTInputEvent event;
    llzSimulatedMouseJustPressed = false;
    llzSimulatedMouseJustReleased = false;
    llzSimulatedScrollWheel = 0.0f;

    while (CTInputPollEvent(&event)) {
        switch (event.type) {
            case CT_EVENT_BUTTON_PRESS:
                if (event.button.button == CT_BUTTON_BACK) {
                    state->backPressed = true;
                    g_backButtonDown = true;
                }
                else if (event.button.button == CT_BUTTON_SELECT) {
                    if (!g_selectButtonDown) {
                        g_selectButtonDown = true;
                        g_selectButtonDownStartTime = GetTime();
                        g_selectHoldReported = false;
                    }
                }
                else if (event.button.button == CT_BUTTON_1) {
                    HandleButtonPress(0);
                    state->upPressed = true;
                }
                else if (event.button.button == CT_BUTTON_2) {
                    HandleButtonPress(1);
                    state->downPressed = true;
                }
                else if (event.button.button == CT_BUTTON_3) {
                    HandleButtonPress(2);
                    state->displayModeNext = true;
                }
                else if (event.button.button == CT_BUTTON_4) {
                    HandleButtonPress(3);
                    state->styleCyclePressed = true;
                }
                else if (event.button.button == CT_BUTTON_SCREENSHOT) {
                    state->screenshotPressed = true;
                    HandleButtonPress(5);  // Map to button6 (index 5), not button5
                }
                else if (event.button.button == 2) state->displayModeNext = true;
                else if (event.button.button == 3) state->styleCyclePressed = true;
                break;
            case CT_EVENT_BUTTON_RELEASE:
                if (event.button.button == CT_BUTTON_BACK) {
                    if (g_backButtonDown) {
                        state->backReleased = true;
                        g_backButtonDown = false;
                    }
                }
                else if (event.button.button == CT_BUTTON_SELECT) {
                    if (g_selectButtonDown) {
                        double holdTime = GetTime() - g_selectButtonDownStartTime;
                        // Quick release = click, not hold
                        if (holdTime < BUTTON_HOLD_THRESHOLD) {
                            state->selectPressed = true;
                            state->playPausePressed = true;
                        }
                        g_selectButtonDown = false;
                        g_selectButtonDownStartTime = 0;
                        g_selectHoldReported = false;
                    }
                }
                else if (event.button.button == CT_BUTTON_1) HandleButtonRelease(0, state);
                else if (event.button.button == CT_BUTTON_2) HandleButtonRelease(1, state);
                else if (event.button.button == CT_BUTTON_3) HandleButtonRelease(2, state);
                else if (event.button.button == CT_BUTTON_4) HandleButtonRelease(3, state);
                else if (event.button.button == CT_BUTTON_SCREENSHOT) HandleButtonRelease(5, state);  // Map to button6
                break;
            case CT_EVENT_SCROLL:
                state->scrollDelta = (float)event.scroll.delta;
                llzSimulatedScrollWheel = (float)event.scroll.delta;
                break;
            case CT_EVENT_TOUCH_PRESS:
                llzSimulatedMousePressed = true;
                llzSimulatedMouseJustPressed = true;
                llzSimulatedMousePos = (Vector2){(float)event.touch.x, (float)event.touch.y};
                g_touchActive = true;
                g_holdReported = false;
                g_touchStartTime = GetTime();
                g_touchStartPos = llzSimulatedMousePos;
                g_dragStartPos = llzSimulatedMousePos;
                g_prevDragPos = llzSimulatedMousePos;
                state->dragActive = true;
                state->dragStart = g_dragStartPos;
                state->dragCurrent = llzSimulatedMousePos;
                state->dragDelta = (Vector2){0};
                break;
            case CT_EVENT_TOUCH_MOVE:
                llzSimulatedMousePos = (Vector2){(float)event.touch.x, (float)event.touch.y};
                if (g_touchActive) {
                    state->dragActive = true;
                    state->dragStart = g_dragStartPos;
                    state->dragCurrent = llzSimulatedMousePos;
                    state->dragDelta = (Vector2){llzSimulatedMousePos.x - g_prevDragPos.x,
                                                llzSimulatedMousePos.y - g_prevDragPos.y};
                    g_prevDragPos = llzSimulatedMousePos;
                }
                break;
            case CT_EVENT_TOUCH_RELEASE:
                llzSimulatedMousePressed = false;
                llzSimulatedMouseJustReleased = true;
                state->dragActive = false;
                state->dragDelta = (Vector2){0};
                if (g_touchActive) {
                    ProcessGestureRelease(state, &llzSimulatedMousePos);
                }
                g_touchActive = false;
                break;
            default:
                break;
        }
    }

    state->mousePos = llzSimulatedMousePos;
    state->mousePressed = llzSimulatedMousePressed;
    state->mouseJustPressed = llzSimulatedMouseJustPressed;
    state->mouseJustReleased = llzSimulatedMouseJustReleased;

    // Update button states (hold detection, timing, etc.)
    UpdateButtonStates(state, GetFrameTime());
    UpdateSelectButtonState(state);
#else
    if (IsKeyPressed(KEY_ESCAPE)) {
        state->backPressed = true;
        g_backButtonDown = true;
    }
    if (IsKeyReleased(KEY_ESCAPE)) {
        if (g_backButtonDown) {
            state->backReleased = true;
            g_backButtonDown = false;
        }
    }
    if (IsKeyPressed(KEY_ENTER)) {
        if (!g_selectButtonDown) {
            g_selectButtonDown = true;
            g_selectButtonDownStartTime = GetTime();
            g_selectHoldReported = false;
        }
    }
    if (IsKeyReleased(KEY_ENTER)) {
        if (g_selectButtonDown) {
            double holdTime = GetTime() - g_selectButtonDownStartTime;
            if (holdTime < BUTTON_HOLD_THRESHOLD) {
                state->selectPressed = true;
                state->playPausePressed = true;
            }
            g_selectButtonDown = false;
            g_selectButtonDownStartTime = 0;
            g_selectHoldReported = false;
        }
    }
    // Handle button presses
    if (IsKeyPressed(KEY_ONE)) {
        HandleButtonPress(0);
        state->upPressed = true;
    }
    if (IsKeyPressed(KEY_TWO)) {
        HandleButtonPress(1);
        state->downPressed = true;
    }
    if (IsKeyPressed(KEY_THREE)) {
        HandleButtonPress(2);
        state->displayModeNext = true;
    }
    if (IsKeyPressed(KEY_FOUR)) {
        HandleButtonPress(3);
        state->styleCyclePressed = true;
    }
    if (IsKeyPressed(KEY_FIVE)) {
        HandleButtonPress(5);  // Map to button6 for consistency with CarThing
        state->screenshotPressed = true;
    }

    // Handle button releases
    if (IsKeyReleased(KEY_ONE)) HandleButtonRelease(0, state);
    if (IsKeyReleased(KEY_TWO)) HandleButtonRelease(1, state);
    if (IsKeyReleased(KEY_THREE)) HandleButtonRelease(2, state);
    if (IsKeyReleased(KEY_FOUR)) HandleButtonRelease(3, state);
    if (IsKeyReleased(KEY_FIVE)) HandleButtonRelease(5, state);
    state->screenshotPressed = state->screenshotPressed || IsKeyPressed(KEY_F1);
    state->displayModeNext = state->displayModeNext || IsKeyPressed(KEY_M);
    state->styleCyclePressed = state->styleCyclePressed || IsKeyPressed(KEY_B);
    state->scrollDelta = GetMouseWheelMove();
    state->mousePos = GetMousePosition();
    state->mousePressed = IsMouseButtonDown(MOUSE_LEFT_BUTTON);
    state->mouseJustPressed = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    state->mouseJustReleased = IsMouseButtonReleased(MOUSE_LEFT_BUTTON);

    if (state->mouseJustPressed) {
        g_touchActive = true;
        g_holdReported = false;
        g_touchStartTime = GetTime();
        g_touchStartPos = state->mousePos;
        g_dragStartPos = state->mousePos;
        g_prevDragPos = state->mousePos;
        state->dragActive = true;
        state->dragStart = g_dragStartPos;
        state->dragCurrent = state->mousePos;
        state->dragDelta = (Vector2){0};
    }
    if (state->mouseJustReleased && g_touchActive) {
        ProcessGestureRelease(state, &state->mousePos);
        g_touchActive = false;
        state->dragActive = false;
        state->dragDelta = (Vector2){0};
    }

    // Update button states (hold detection, timing, etc.)
    UpdateButtonStates(state, GetFrameTime());
    UpdateSelectButtonState(state);
#endif

    if (g_touchActive) {
        MaybeReportHold(state, &state->mousePos);
        if (!state->dragActive) {
            state->dragActive = true;
            state->dragStart = g_dragStartPos;
            state->dragCurrent = llzSimulatedMousePos;
            state->dragDelta = (Vector2){llzSimulatedMousePos.x - g_prevDragPos.x,
                                         llzSimulatedMousePos.y - g_prevDragPos.y};
            g_prevDragPos = llzSimulatedMousePos;
        }
    } else {
        state->dragActive = false;
        state->dragDelta = (Vector2){0};
    }

    state->doubleClick = state->doubleTap;
    state->longPress = state->hold;
    g_state = *state;
}

void LlzInputShutdown(void)
{
#ifdef PLATFORM_DRM
    CTInputClose();
#endif
}

const LlzInputState *LlzInputGetState(void)
{
    return &g_state;
}
