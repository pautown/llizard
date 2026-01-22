#ifndef LLZ_SDK_INPUT_H
#define LLZ_SDK_INPUT_H

#include <stdbool.h>
#include "raylib.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool backPressed;
    bool backReleased;    // Back button just released
    bool backDown;        // Back button currently held
    bool backHold;        // Back held past threshold (edge-triggered)
    float backHoldTime;   // Duration held in seconds
    bool backClick;       // Back quick click (released before hold threshold)
    bool selectPressed;
    bool selectDown;        // Select button currently held
    bool selectHold;        // Select held past threshold (edge-triggered)
    float selectHoldTime;   // Duration held in seconds
    bool upPressed;
    bool downPressed;
    bool screenshotPressed;  // CT_BUTTON_SCREENSHOT (code 50/5) - used for clock toggle
    bool displayModeNext;    // CT button code 2 cycles plugin display layouts
    bool styleCyclePressed;  // CT button code 3 cycles background styles

    // Button states: Down = currently held, Pressed = quick click, Hold = held past threshold
    bool button1Pressed;
    bool button1Down;
    bool button1Hold;
    float button1HoldTime;

    bool button2Pressed;
    bool button2Down;
    bool button2Hold;
    float button2HoldTime;

    bool button3Pressed;
    bool button3Down;
    bool button3Hold;
    float button3HoldTime;

    bool button4Pressed;
    bool button4Down;
    bool button4Hold;
    float button4HoldTime;

    bool button5Pressed;
    bool button5Down;
    bool button5Hold;
    float button5HoldTime;

    bool button6Pressed;     // Quick click on button code 5
    bool button6Down;        // Currently held
    bool button6Hold;        // Held past threshold (edge-triggered)
    float button6HoldTime;   // Duration held in seconds

    bool playPausePressed;
    float scrollDelta;
    Vector2 mousePos;
    bool mousePressed;
    bool mouseJustPressed;
    bool mouseJustReleased;
    bool tap;
    bool doubleTap;
    bool doubleClick;
    bool hold;
    bool longPress;
    Vector2 tapPosition;
    Vector2 holdPosition;
    bool dragActive;
    Vector2 dragStart;
    Vector2 dragCurrent;
    Vector2 dragDelta;
    Vector2 swipeDelta;
    Vector2 swipeStart;
    Vector2 swipeEnd;
    bool swipeLeft;
    bool swipeRight;
    bool swipeUp;
    bool swipeDown;
} LlzInputState;

void LlzInputInit(void);
void LlzInputUpdate(LlzInputState *state);
void LlzInputShutdown(void);
const LlzInputState *LlzInputGetState(void);

extern bool llzSimulatedMousePressed;
extern bool llzSimulatedMouseJustPressed;
extern bool llzSimulatedMouseJustReleased;
extern Vector2 llzSimulatedMousePos;
extern float llzSimulatedScrollWheel;

#ifdef __cplusplus
}
#endif

#endif
