#include "nowplaying/core/np_effects.h"
#include <string.h>

void NpEffectInit(NpEffect *effect) {
    memset(effect, 0, sizeof(NpEffect));
    effect->type = NP_EFFECT_NONE;
    effect->progress = 1.0f;
    effect->duration = 0.3f;
}

void NpEffectStart(NpEffect *effect, NpEffectType type, float duration) {
    effect->type = type;
    effect->active = true;
    effect->progress = 0.0f;
    effect->elapsed = 0.0f;
    effect->duration = duration > 0.0f ? duration : 0.3f;
}

void NpEffectUpdate(NpEffect *effect, float deltaTime) {
    if (!effect->active) return;

    effect->elapsed += deltaTime;
    effect->progress = effect->elapsed / effect->duration;

    if (effect->progress >= 1.0f) {
        effect->progress = 1.0f;
        effect->active = false;
    }
}

bool NpEffectIsActive(NpEffect *effect) {
    return effect->active;
}

bool NpEffectIsFinished(NpEffect *effect) {
    return !effect->active && effect->progress >= 1.0f;
}

float NpEffectGetAlpha(NpEffect *effect) {
    float t = NpEaseInOutCubic(effect->progress);

    switch (effect->type) {
        case NP_EFFECT_FADE_IN:
            return t;
        case NP_EFFECT_FADE_OUT:
            return 1.0f - t;
        default:
            return 1.0f;
    }
}

float NpEaseInOutCubic(float t) {
    if (t < 0.5f) {
        return 4.0f * t * t * t;
    } else {
        float f = (2.0f * t) - 2.0f;
        return 0.5f * f * f * f + 1.0f;
    }
}
