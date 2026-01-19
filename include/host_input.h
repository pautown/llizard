#ifndef HOST_INPUT_H
#define HOST_INPUT_H

#include <stdbool.h>
#include "raylib.h"

typedef struct {
    bool backPressed;
    bool selectPressed;
    bool upPressed;
    bool downPressed;
    bool screenshotPressed;  // CT_BUTTON_SCREENSHOT (code 50/5) - used for clock toggle
    bool displayModeNext;    // CT button code 2 cycles plugin layouts
    bool styleCyclePressed;  // CT button code 3 cycles background styles
    bool button1Pressed;
    bool button2Pressed;
    bool button3Pressed;
    bool button4Pressed;
    bool button5Pressed;
    bool playPausePressed;   // CT button select toggles playback
    float scrollDelta;
    Vector2 mousePos;
    bool mousePressed;
    bool mouseJustPressed;
    bool mouseJustReleased;
} HostInputState;

void HostInputInit(void);
void HostInputUpdate(HostInputState *state);
void HostInputShutdown(void);

const HostInputState *HostInputGetState(void);

// expose simulated mouse globals for plugins needing raygui hooks
extern bool hostSimulatedMousePressed;
extern bool hostSimulatedMouseJustPressed;
extern bool hostSimulatedMouseJustReleased;
extern Vector2 hostSimulatedMousePos;
extern float hostSimulatedScrollWheel;

#endif
