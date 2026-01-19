#ifndef NP_PLUGIN_EFFECTS_H
#define NP_PLUGIN_EFFECTS_H

#include <stdbool.h>

typedef enum {
    NP_EFFECT_NONE,
    NP_EFFECT_FADE_IN,
    NP_EFFECT_FADE_OUT
} NpEffectType;

typedef struct {
    NpEffectType type;
    bool active;
    float progress;  // 0.0 to 1.0
    float duration;
    float elapsed;
} NpEffect;

void NpEffectInit(NpEffect *effect);
void NpEffectStart(NpEffect *effect, NpEffectType type, float duration);
void NpEffectUpdate(NpEffect *effect, float deltaTime);
bool NpEffectIsActive(NpEffect *effect);
bool NpEffectIsFinished(NpEffect *effect);
float NpEffectGetAlpha(NpEffect *effect);  // Returns opacity 0.0-1.0
float NpEaseInOutCubic(float t);

#endif // NP_PLUGIN_EFFECTS_H
