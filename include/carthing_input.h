#ifndef CARTHING_INPUT_H
#define CARTHING_INPUT_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CT_EVENT_NONE = 0,
    CT_EVENT_BUTTON_PRESS,
    CT_EVENT_BUTTON_RELEASE,
    CT_EVENT_SCROLL,
    CT_EVENT_TOUCH_PRESS,
    CT_EVENT_TOUCH_MOVE,
    CT_EVENT_TOUCH_RELEASE
} CTEventType;

typedef enum {
    CT_BUTTON_NONE = 0,
    CT_BUTTON_BACK,
    CT_BUTTON_SELECT,
    CT_BUTTON_1,
    CT_BUTTON_2,
    CT_BUTTON_3,
    CT_BUTTON_4,
    CT_BUTTON_SCREENSHOT
} CTButton;

typedef struct {
    CTEventType type;
    union {
        struct {
            CTButton button;
        } button;
        struct {
            int delta;
        } scroll;
        struct {
            int x;
            int y;
        } touch;
    };
} CTInputEvent;

bool CTInputInit(void);
void CTInputClose(void);
bool CTInputPollEvent(CTInputEvent *event);
bool CTInputIsButtonDown(CTButton button);
bool CTInputGetTouchPosition(int *x, int *y);

#ifdef __cplusplus
}
#endif

#endif
