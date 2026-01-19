#ifndef LLZ_SDK_DISPLAY_H
#define LLZ_SDK_DISPLAY_H

#include <stdbool.h>

#include "raylib.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LLZ_LOGICAL_WIDTH 800
#define LLZ_LOGICAL_HEIGHT 480

bool LlzDisplayInit(void);
void LlzDisplayBegin(void);
void LlzDisplayEnd(void);
void LlzDisplayShutdown(void);

#ifdef __cplusplus
}
#endif

#endif
