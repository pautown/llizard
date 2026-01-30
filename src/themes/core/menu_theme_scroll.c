#include "menu_theme_private.h"
#include "llz_sdk.h"
#include <math.h>

#define SCREEN_WIDTH LLZ_LOGICAL_WIDTH
#define SCREEN_HEIGHT LLZ_LOGICAL_HEIGHT
#define MENU_VISIBLE_AREA (SCREEN_HEIGHT - MENU_PADDING_TOP)

void MenuThemeScrollInit(MenuThemeScrollState *scroll)
{
    scroll->scrollOffset = 0.0f;
    scroll->targetScrollOffset = 0.0f;
    scroll->carouselOffset = 0.0f;
    scroll->carouselTarget = 0.0f;
}

// Calculate scroll offset to keep selection visible (list/grid styles)
float MenuThemeScrollCalculateTarget(int selected, int count, float currentTarget)
{
    if (count == 0) return 0.0f;

    float itemTotalHeight = MENU_ITEM_HEIGHT + MENU_ITEM_SPACING;
    float totalListHeight = count * itemTotalHeight;
    float maxScroll = totalListHeight - MENU_VISIBLE_AREA;
    if (maxScroll < 0) maxScroll = 0;

    // Calculate where the selected item is
    float selectedTop = selected * itemTotalHeight;
    float selectedBottom = selectedTop + MENU_ITEM_HEIGHT;

    // Visible window based on current target scroll
    float visibleTop = currentTarget;
    float visibleBottom = currentTarget + MENU_VISIBLE_AREA;

    // Margins to keep selection away from edges
    float topMargin = MENU_ITEM_HEIGHT * 0.5f;
    float bottomMargin = MENU_ITEM_HEIGHT * 1.2f;

    float newTarget = currentTarget;

    // If selected item is above visible area, scroll up
    if (selectedTop < visibleTop + topMargin) {
        newTarget = selectedTop - topMargin;
    }
    // If selected item is below visible area, scroll down
    else if (selectedBottom > visibleBottom - bottomMargin) {
        newTarget = selectedBottom - MENU_VISIBLE_AREA + bottomMargin;
    }

    // Clamp to valid range
    if (newTarget < 0) newTarget = 0;
    if (newTarget > maxScroll) newTarget = maxScroll;

    return newTarget;
}

// Update smooth scroll animation
void MenuThemeScrollUpdate(MenuThemeScrollState *scroll, float deltaTime)
{
    float diff = scroll->targetScrollOffset - scroll->scrollOffset;
    float speed = 12.0f;

    // Smooth interpolation
    scroll->scrollOffset += diff * speed * deltaTime;

    // Snap when very close
    if (fabsf(diff) < 0.5f) {
        scroll->scrollOffset = scroll->targetScrollOffset;
    }
}

// Update carousel horizontal scroll
void MenuThemeScrollUpdateCarousel(MenuThemeScrollState *scroll, float deltaTime)
{
    float diff = scroll->carouselTarget - scroll->carouselOffset;
    float speed = 10.0f;

    scroll->carouselOffset += diff * speed * deltaTime;

    if (fabsf(diff) < 0.5f) {
        scroll->carouselOffset = scroll->carouselTarget;
    }
}
