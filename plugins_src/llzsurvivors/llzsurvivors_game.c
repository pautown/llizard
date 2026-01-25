// LLZ Survivors - Game Logic Implementation
// Vampire Survivors/Brotato-lite arena survival game

#include "llzsurvivors_game.h"
#include "llz_sdk.h"
#include "rlgl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// =============================================================================
// GLOBALS
// =============================================================================

static Game g_game;
static int g_screenWidth = SCREEN_WIDTH;
static int g_screenHeight = SCREEN_HEIGHT;
static bool g_wantsClose = false;
static Font g_font;

// =============================================================================
// JUICE/POLISH EFFECT STATE
// =============================================================================

// Hitstop (brief freeze on kills)
static float g_hitstopTimer = 0.0f;
#define HITSTOP_DURATION 0.025f  // ~25ms freeze frame

// Level up celebration
static float g_levelUpCelebration = 0.0f;  // Timer for celebration effects
static float g_levelUpFreeze = 0.0f;       // Time freeze during level up
static Vector2 g_levelUpPos = {0};         // Position of level up burst
#define LEVEL_UP_FREEZE_DURATION 0.15f     // Brief pause on level up
#define LEVEL_UP_BURST_PARTICLES 24        // Radial burst count

// Damage vignette (red screen edges on hit)
static float g_damageVignette = 0.0f;      // Intensity of vignette
#define VIGNETTE_FADE_SPEED 3.0f           // How fast vignette fades

// Last crit tracking for colored damage numbers
static bool g_lastHitWasCrit = false;

// =============================================================================
// PHASE 2 JUICE EFFECTS
// =============================================================================

// Kill streak system
static int g_killStreak = 0;               // Current consecutive kills
static float g_killStreakTimer = 0.0f;     // Time since last kill
static float g_killStreakDisplay = 0.0f;   // Display timer for milestone
static int g_killStreakMilestone = 0;      // Last milestone hit
#define KILL_STREAK_TIMEOUT 2.0f           // Streak resets after this
#define KILL_STREAK_DISPLAY_TIME 1.5f      // How long to show milestone

// Kill streak milestones
static const int KILL_MILESTONES[] = {5, 10, 25, 50, 100, 200};
static const char *KILL_MILESTONE_NAMES[] = {"KILLING SPREE!", "RAMPAGE!", "UNSTOPPABLE!", "GODLIKE!", "LEGENDARY!", "IMMORTAL!"};
#define NUM_KILL_MILESTONES 6

// Wave celebration
static int g_lastWave = 0;                 // Track wave changes
static float g_waveCelebration = 0.0f;     // Celebration animation timer
#define WAVE_CELEBRATION_TIME 2.0f

// Enemy death animation pool
#define MAX_DYING_ENEMIES 16
typedef struct {
    Vector2 pos;
    EnemyType type;
    float size;
    float timer;
    float maxTime;
    Color color;
    bool active;
} DyingEnemy;
static DyingEnemy g_dyingEnemies[MAX_DYING_ENEMIES];
#define DEATH_ANIM_TIME 0.25f

// Spawn warning indicators
#define MAX_SPAWN_WARNINGS 8
typedef struct {
    Vector2 worldPos;       // Where enemy will spawn
    float timer;            // Time until spawn
    float maxTime;
    EnemyType type;
    bool active;
} SpawnWarning;
static SpawnWarning g_spawnWarnings[MAX_SPAWN_WARNINGS];
#define SPAWN_WARNING_TIME 0.8f

// =============================================================================
// PHASE 3 POLISH - MENU/UI EFFECTS
// =============================================================================

// Menu animation state
static float g_menuTitleGlow = 0.0f;        // Glow animation timer
static float g_menuButtonScale[2] = {1.0f, 1.0f};  // Button scale for hover effect
static float g_menuEntranceTime = 0.0f;     // Entrance animation progress

// Class select animation (carousel style)
static float g_classSelectEntrance = 0.0f;  // Entrance animation
static float g_classCarouselPos = 0.0f;     // Current carousel position (smooth)
static float g_classCarouselTarget = 0.0f;  // Target carousel position
static float g_classCardGlow[CLASS_COUNT];  // Per-card glow

// Weapon select animation (carousel style like Bejeweled)
static float g_weaponSelectEntrance = 0.0f; // Entrance animation
static float g_weaponCarouselPos = 0.0f;    // Current carousel position (smooth)
static float g_weaponCarouselTarget = 0.0f; // Target carousel position
static float g_weaponCardGlow[STARTING_WEAPON_COUNT];  // Per-card glow

// Game over animation
static float g_gameOverEntrance = 0.0f;     // Slide-in animation
static float g_statCountUp = 0.0f;          // Count-up progress (0-1)
static int g_displayedKills = 0;            // Animated kill counter
static float g_displayedTime = 0.0f;        // Animated time counter

// HP bar effects
static float g_hpFlash = 0.0f;              // Damage flash intensity
static float g_hpPrevValue = 0.0f;          // Track HP changes for flash
static float g_lowHpPulse = 0.0f;           // Low HP pulsing timer

// Screen edge danger glow
static float g_dangerGlow[4] = {0};         // L, R, T, B edge glow intensities
#define DANGER_GLOW_RANGE 200.0f            // Distance for danger detection
#define LOW_HP_THRESHOLD 0.3f               // HP percent for low HP pulse

// Background system active flag
static bool g_bgSystemInitialized = false;

// Forward declaration for utility function used by spatial grid
static float Distance(Vector2 a, Vector2 b);

// Forward declaration for danger zone XP multiplier (used in DamageEnemy before defined)
static float GetDangerZoneXPMultiplier(Vector2 pos);

// =============================================================================
// SPATIAL GRID COLLISION SYSTEM
// =============================================================================

// Grid cell structure - stores indices of enemies in this cell
typedef struct {
    int enemyIndices[MAX_ENTITIES_PER_CELL];
    int count;
} GridCell;

// The spatial grid - covers entire world
static GridCell g_spatialGrid[GRID_WIDTH][GRID_HEIGHT];

// Convert world position to grid cell coordinates
static inline int WorldToGridX(float x) {
    int gx = (int)(x / GRID_CELL_SIZE);
    if (gx < 0) gx = 0;
    if (gx >= GRID_WIDTH) gx = GRID_WIDTH - 1;
    return gx;
}

static inline int WorldToGridY(float y) {
    int gy = (int)(y / GRID_CELL_SIZE);
    if (gy < 0) gy = 0;
    if (gy >= GRID_HEIGHT) gy = GRID_HEIGHT - 1;
    return gy;
}

// Clear all grid cells
static void ClearSpatialGrid(void) {
    for (int x = 0; x < GRID_WIDTH; x++) {
        for (int y = 0; y < GRID_HEIGHT; y++) {
            g_spatialGrid[x][y].count = 0;
        }
    }
}

// Populate grid with current enemy positions
static void PopulateSpatialGrid(void) {
    ClearSpatialGrid();

    for (int i = 0; i < MAX_ENEMIES; i++) {
        Enemy *e = &g_game.enemies[i];
        if (!e->active) continue;

        int gx = WorldToGridX(e->pos.x);
        int gy = WorldToGridY(e->pos.y);

        GridCell *cell = &g_spatialGrid[gx][gy];
        if (cell->count < MAX_ENTITIES_PER_CELL) {
            cell->enemyIndices[cell->count++] = i;
        }
    }
}

// Check collision with enemies near a world position
// Returns index of first hit enemy, or -1 if none
// Also populates hitList with all enemies within range (for pierce/AOE)
static int CheckEnemyCollisionAtPoint(Vector2 pos, float radius, int *hitList, int maxHits, int *hitCount) {
    int firstHit = -1;
    *hitCount = 0;

    // Calculate which grid cells to check (3x3 around the point)
    int gx = WorldToGridX(pos.x);
    int gy = WorldToGridY(pos.y);

    for (int dx = -1; dx <= 1; dx++) {
        for (int dy = -1; dy <= 1; dy++) {
            int cx = gx + dx;
            int cy = gy + dy;
            if (cx < 0 || cx >= GRID_WIDTH || cy < 0 || cy >= GRID_HEIGHT) continue;

            GridCell *cell = &g_spatialGrid[cx][cy];
            for (int i = 0; i < cell->count; i++) {
                int enemyIdx = cell->enemyIndices[i];
                Enemy *e = &g_game.enemies[enemyIdx];
                if (!e->active) continue;

                float dist = Distance(pos, e->pos);
                if (dist < (radius + e->size / 2)) {
                    if (firstHit < 0) firstHit = enemyIdx;
                    if (hitList && *hitCount < maxHits) {
                        hitList[(*hitCount)++] = enemyIdx;
                    }
                }
            }
        }
    }

    return firstHit;
}

// Simpler version - just check if any enemy is within range
static int FindEnemyInRange(Vector2 pos, float radius) {
    int gx = WorldToGridX(pos.x);
    int gy = WorldToGridY(pos.y);

    for (int dx = -1; dx <= 1; dx++) {
        for (int dy = -1; dy <= 1; dy++) {
            int cx = gx + dx;
            int cy = gy + dy;
            if (cx < 0 || cx >= GRID_WIDTH || cy < 0 || cy >= GRID_HEIGHT) continue;

            GridCell *cell = &g_spatialGrid[cx][cy];
            for (int i = 0; i < cell->count; i++) {
                int enemyIdx = cell->enemyIndices[i];
                Enemy *e = &g_game.enemies[enemyIdx];
                if (!e->active) continue;

                if (Distance(pos, e->pos) < (radius + e->size / 2)) {
                    return enemyIdx;
                }
            }
        }
    }

    return -1;
}

// Find nearest enemy within maxRange using spatial grid
// Returns index of nearest enemy or -1 if none found
static int FindNearestEnemyGrid(Vector2 pos, float maxRange) {
    int nearest = -1;
    float nearestDistSq = maxRange * maxRange;

    // Calculate grid cell range based on maxRange
    int cellRadius = (int)(maxRange / GRID_CELL_SIZE) + 1;
    int gx = WorldToGridX(pos.x);
    int gy = WorldToGridY(pos.y);

    // Check cells within range
    for (int dx = -cellRadius; dx <= cellRadius; dx++) {
        for (int dy = -cellRadius; dy <= cellRadius; dy++) {
            int cx = gx + dx;
            int cy = gy + dy;
            if (cx < 0 || cx >= GRID_WIDTH || cy < 0 || cy >= GRID_HEIGHT) continue;

            GridCell *cell = &g_spatialGrid[cx][cy];
            for (int i = 0; i < cell->count; i++) {
                int enemyIdx = cell->enemyIndices[i];
                Enemy *e = &g_game.enemies[enemyIdx];
                if (!e->active) continue;

                // Use squared distance to avoid sqrt
                float dx2 = pos.x - e->pos.x;
                float dy2 = pos.y - e->pos.y;
                float distSq = dx2 * dx2 + dy2 * dy2;
                if (distSq < nearestDistSq) {
                    nearestDistSq = distSq;
                    nearest = enemyIdx;
                }
            }
        }
    }

    return nearest;
}

// =============================================================================
// ENEMY POOL PROGRESSION SYSTEM
// =============================================================================

// Tracks which enemy types are in the spawn pool
static bool g_enemyPoolUnlocked[ENEMY_TYPE_COUNT] = {true, false, false, false, false, false, false, false};
// Base enemies: Walker always unlocked, Fast at wave 1, Tank at wave 3

// Enemy introduction announcement
static float g_enemyIntroTimer = 0.0f;
static EnemyType g_enemyIntroType = ENEMY_WALKER;
static bool g_enemyIntroActive = false;
#define ENEMY_INTRO_TIME 3.0f

// Enemy unlock waves (must match EnemyType enum order)
static const int ENEMY_UNLOCK_WAVES[] = {
    0,   // WALKER - always
    1,   // FAST - wave 1
    3,   // TANK - wave 3
    5,   // SWARM - wave 5
    7,   // ELITE - wave 7
    8,   // HORNET - wave 8
    10,  // BRUTE - wave 10
    12,  // SPINNER - wave 12
    13,  // MIRROR - wave 13
    14,  // SHIELDER - wave 14
    15,  // BOSS - wave 15
    16,  // BOMBER - wave 16
    18   // PHASER - wave 18
};

static const char *ENEMY_NAMES[] = {
    "WALKER", "SPEEDSTER", "TANK", "SWARM", "ELITE", "HORNET", "BRUTE",
    "SPINNER", "MIRROR", "SHIELDER", "BOSS", "BOMBER", "PHASER"
};

static const char *ENEMY_DESCRIPTIONS[] = {
    "Basic enemy",
    "Fast and nimble",
    "Slow but tough",
    "Tiny and numerous",
    "Enhanced warrior",
    "Ranged laser attacker",
    "Heavy hitter",
    "Spiral bullet storm",
    "Which is real?",
    "Shield blocks attacks",
    "Massive threat",
    "Drops explosive mines",
    "Phases in and out"
};

// Forward declarations
static void GenerateUpgradeChoices(void);
static void DamagePlayer(int damage, Vector2 knockbackFrom);
static bool HasShield(void);

// XP thresholds per level
static const int XP_THRESHOLDS[] = {
    20, 40, 70, 110, 160, 220, 300, 400, 520, 660,
    820, 1000, 1200, 1420, 1660, 1920, 2200, 2500, 2820, 3160
};
#define MAX_LEVEL 20

// Skill tier costs (points needed to reach each tier)
static const int SKILL_TIER_COSTS[] = {1, 1, 2, 2, 3};  // Tiers 1-5

// Weapon names and descriptions
static const char *WEAPON_NAMES[] = {
    "Melee", "Distance", "Magic", "Radius", "Mystic",
    "Seeker", "Boomerang", "Venom", "Chain"
};
static const char *WEAPON_DESCS[] = {
    "Close-range arc attack",
    "Fires bullets forward",
    "Expanding damage wave",
    "Orbiting damage orbs",
    "Random lightning strikes",
    "Homing missiles",
    "Returning blade",
    "Toxic poison clouds",
    "Chain lightning"
};

// Branch information for each weapon (3 branches per starting weapon)
typedef struct {
    const char *name;
    const char *desc;
    const char *tierDescs[MAX_BRANCH_TIER];
    bool isOffensive;
} BranchInfo;

// Melee branches
static const BranchInfo MELEE_BRANCHES[4] = {
    {"None", "No specialization", {NULL}, true},
    {"Wide Arc", "Sweep wider, faster",
     {"Arc +45deg", "Arc +90deg, 2 swings", "Arc 180deg", "Arc 270deg", "360deg sweep"}, true},
    {"Power Strike", "Heavy damage, knockback",
     {"Dmg x1.5", "Dmg x2, knockback", "Dmg x2.5, stun", "Dmg x3, big stun", "Execute <20%"}, true},
    {"Blade Storm", "Continuous spin attack",
     {"Spin 0.5s/3s", "Spin 1s/3s", "Spin 1.5s/2.5s", "Spin 2s/2s", "Always spin"}, true}
};

// Distance branches
static const BranchInfo DISTANCE_BRANCHES[4] = {
    {"None", "No specialization", {NULL}, true},
    {"Rapid Fire", "More bullets, faster",
     {"+50% rate", "+2 bullets", "+100% rate", "+3 bullets", "Bullet storm"}, true},
    {"Piercing", "Bullets pass through",
     {"Pierce 1", "Pierce 2, +dmg", "Pierce 3", "Pierce all", "Railgun"}, true},
    {"Spread Shot", "Shotgun-style fan",
     {"3-bullet fan", "5-bullet fan", "7 tight spread", "9 bullets", "12 nova"}, true}
};

// Magic branches
static const BranchInfo MAGIC_BRANCHES[4] = {
    {"None", "No specialization", {NULL}, true},
    {"Nova Blast", "Larger, stronger pulses",
     {"+50% radius", "+100% radius", "+150% radius", "2 waves", "Mega nova"}, true},
    {"Pulse Storm", "Rapid small pulses",
     {"2 pulses", "3 pulses", "4 pulses", "5 pulses", "Continuous"}, true},
    {"Frost Wave", "Slow and freeze enemies",
     {"30% slow 2s", "50% slow 3s", "70% slow", "Freeze 1s", "Shatter +dmg"}, false}
};

// Radius branches
static const BranchInfo RADIUS_BRANCHES[4] = {
    {"None", "No specialization", {NULL}, true},
    {"Guardian", "Block attacks, defensive",
     {"Block 1 hit", "Block 2, heal", "Block 3, reflect", "Block 5, regen", "Invincible"}, false},
    {"Swarm", "Many small fast orbs",
     {"+3 tiny orbs", "+5 orbs, +spd", "+7 orbs", "+10 tracking", "20 orb swarm"}, true},
    {"Heavy Orbs", "Few devastating orbs",
     {"2 large +dmg", "x2 dmg, knock", "x3 dmg, stun", "1 huge, x5 dmg", "Orbital cannon"}, true}
};

// Mystic branches
static const BranchInfo MYSTIC_BRANCHES[4] = {
    {"None", "No specialization", {NULL}, true},
    {"Chain", "Bounces between enemies",
     {"Chain to 2", "Chain 3, +dmg", "Chain to 5", "Chain to 8", "Arc web"}, true},
    {"Storm", "Random strikes in area",
     {"2 strikes", "3 strikes, wider", "5 strikes", "8 strikes", "Lightning field"}, true},
    {"Smite", "Single powerful strike",
     {"x2 dmg, nearest", "x3 dmg, strongest", "x5 dmg, mark", "x7 dmg, execute", "Annihilate"}, true}
};

static const BranchInfo *GetBranchInfo(WeaponType weapon, int branch) {
    if (branch < 0 || branch > 3) return NULL;
    switch (weapon) {
        case WEAPON_MELEE: return &MELEE_BRANCHES[branch];
        case WEAPON_DISTANCE: return &DISTANCE_BRANCHES[branch];
        case WEAPON_MAGIC: return &MAGIC_BRANCHES[branch];
        case WEAPON_RADIUS: return &RADIUS_BRANCHES[branch];
        case WEAPON_MYSTIC: return &MYSTIC_BRANCHES[branch];
        default: return NULL;
    }
}

// Upgrade type info (for random selection)
typedef struct {
    UpgradeType type;
    const char *name;
    const char *descTemplate;  // %d for value placeholder
    int baseValue;
    int cost;
    bool isOffensive;
} UpgradeInfo;

static const UpgradeInfo UPGRADE_POOL[] = {
    // Offensive (first 7)
    {UPGRADE_WEAPON_TIER,     "Weapon+",      "Upgrade weapon tier",           0,  1, true},
    {UPGRADE_WEAPON_UNLOCK,   "New Weapon",   "Unlock a new weapon",           0,  2, true},
    {UPGRADE_DAMAGE_ALL,      "Damage+",      "+%d%% all damage",             10,  1, true},
    {UPGRADE_ATTACK_SPEED,    "Atk Speed+",   "+%d%% attack speed",           10,  1, true},
    {UPGRADE_CRIT_CHANCE,     "Crit+",        "+%d%% crit chance",             5,  1, true},
    {UPGRADE_AREA_SIZE,       "Area+",        "+%d%% attack area",            15,  1, true},
    {UPGRADE_PROJECTILE_COUNT,"Projectile+",  "+1 projectile/orb",             1,  2, true},

    // Defensive (next 8)
    {UPGRADE_MAX_HP,          "Max HP+",      "+%d max HP",                   20,  1, false},
    {UPGRADE_HEALTH_REGEN,    "Regen+",       "+%d HP/s when still",           3,  1, false},
    {UPGRADE_MOVE_SPEED,      "Speed+",       "+%d%% move speed",             12,  1, false},
    {UPGRADE_MAGNET_RANGE,    "Magnet+",      "+%d%% XP range",               25,  1, false},
    {UPGRADE_ARMOR,           "Armor+",       "+%d%% damage resist",           8,  1, false},
    {UPGRADE_LIFESTEAL,       "Lifesteal+",   "+%d%% damage->HP",              5,  1, false},
    {UPGRADE_DODGE_CHANCE,    "Dodge+",       "+%d%% dodge chance",            5,  1, false},
    {UPGRADE_THORNS,          "Thorns+",      "+%d%% dmg reflect",            15,  1, false},
};

// =============================================================================
// WEAPON SYNERGY SYSTEM
// =============================================================================

// Synergy definitions - weapon combinations that provide bonuses
typedef struct {
    WeaponType weapon1;
    WeaponType weapon2;
    const char *name;
    const char *desc;
    float damageBonus;      // Multiplier to damage (e.g., 1.2 = +20%)
    float speedBonus;       // Cooldown reduction (e.g., 0.9 = 10% faster)
    float areaBonus;        // Area increase (e.g., 1.15 = +15% area)
    int bonusProjectiles;   // Extra projectiles for applicable weapons
} WeaponSynergy;

static const WeaponSynergy WEAPON_SYNERGIES[] = {
    // Elemental synergies
    {WEAPON_MELEE, WEAPON_MAGIC, "Arcane Blade", "Melee triggers mini-waves", 1.15f, 1.0f, 1.0f, 0},
    {WEAPON_DISTANCE, WEAPON_RADIUS, "Orbital Fire", "Orbs boost bullet damage", 1.2f, 1.0f, 1.0f, 0},
    {WEAPON_MYSTIC, WEAPON_CHAIN, "Storm Master", "Lightning chains further", 1.0f, 0.85f, 1.25f, 0},
    {WEAPON_POISON, WEAPON_MAGIC, "Toxic Wave", "Waves spread poison", 1.0f, 1.0f, 1.3f, 0},
    {WEAPON_SEEKER, WEAPON_BOOMERANG, "Guided Arsenal", "Projectiles home better", 1.1f, 0.9f, 1.0f, 0},

    // Offense synergies
    {WEAPON_MELEE, WEAPON_DISTANCE, "Gun & Blade", "Attack speed boost", 1.0f, 0.8f, 1.0f, 1},
    {WEAPON_RADIUS, WEAPON_MYSTIC, "Elemental Master", "All damage increased", 1.25f, 1.0f, 1.0f, 0},

    // Defense synergies
    {WEAPON_POISON, WEAPON_CHAIN, "Spreading Doom", "Slow spreads on chain", 1.0f, 1.0f, 1.2f, 0},
};

#define NUM_SYNERGIES (sizeof(WEAPON_SYNERGIES) / sizeof(WEAPON_SYNERGIES[0]))

// Check if a synergy is active (both weapons unlocked)
static bool IsSynergyActive(const WeaponSynergy *syn) {
    return g_game.weapons[syn->weapon1].tier > 0 && g_game.weapons[syn->weapon2].tier > 0;
}

// Get total synergy bonuses for a specific weapon
static void GetSynergyBonuses(WeaponType weapon, float *damage, float *speed, float *area, int *projectiles) {
    *damage = 1.0f;
    *speed = 1.0f;
    *area = 1.0f;
    *projectiles = 0;

    for (int i = 0; i < (int)NUM_SYNERGIES; i++) {
        const WeaponSynergy *syn = &WEAPON_SYNERGIES[i];
        if (!IsSynergyActive(syn)) continue;

        // Apply bonus if this weapon is part of the synergy
        if (syn->weapon1 == weapon || syn->weapon2 == weapon) {
            *damage *= syn->damageBonus;
            *speed *= syn->speedBonus;
            *area *= syn->areaBonus;
            *projectiles += syn->bonusProjectiles;
        }
    }
}

// Count active synergies (for UI display)
static int CountActiveSynergies(void) {
    int count = 0;
    for (int i = 0; i < (int)NUM_SYNERGIES; i++) {
        if (IsSynergyActive(&WEAPON_SYNERGIES[i])) count++;
    }
    return count;
}

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

static float Clampf(float v, float min, float max) {
    if (v < min) return min;
    if (v > max) return max;
    return v;
}

static float Lerpf(float a, float b, float t) {
    return a + (b - a) * Clampf(t, 0.0f, 1.0f);
}

static float Distance(Vector2 a, Vector2 b) {
    float dx = b.x - a.x;
    float dy = b.y - a.y;
    return sqrtf(dx * dx + dy * dy);
}

static Vector2 Normalize(Vector2 v) {
    float len = sqrtf(v.x * v.x + v.y * v.y);
    if (len < 0.0001f) return (Vector2){0, 0};
    return (Vector2){v.x / len, v.y / len};
}

static float RandomFloat(float min, float max) {
    return min + (float)GetRandomValue(0, 10000) / 10000.0f * (max - min);
}

static float AngleDiff(float a, float b) {
    float diff = fmodf(b - a + PI, PI * 2) - PI;
    return diff < -PI ? diff + PI * 2 : diff;
}

// =============================================================================
// EASING FUNCTIONS (for juicy animations)
// =============================================================================

static float EaseOutBack(float t) {
    const float c1 = 1.70158f;
    const float c3 = c1 + 1.0f;
    return 1.0f + c3 * powf(t - 1.0f, 3.0f) + c1 * powf(t - 1.0f, 2.0f);
}

static float EaseOutElastic(float t) {
    if (t == 0 || t == 1) return t;
    const float c4 = (2.0f * PI) / 3.0f;
    return powf(2.0f, -10.0f * t) * sinf((t * 10.0f - 0.75f) * c4) + 1.0f;
}

static float EaseOutQuad(float t) {
    return 1.0f - (1.0f - t) * (1.0f - t);
}

static float EaseInOutCubic(float t) {
    return t < 0.5f ? 4.0f * t * t * t : 1.0f - powf(-2.0f * t + 2.0f, 3.0f) / 2.0f;
}

// =============================================================================
// CAMERA SYSTEM
// =============================================================================

static Vector2 WorldToScreen(Vector2 worldPos) {
    return (Vector2){
        worldPos.x - g_game.camera.pos.x + g_screenWidth / 2.0f,
        worldPos.y - g_game.camera.pos.y + g_screenHeight / 2.0f
    };
}

static bool IsOnScreen(Vector2 worldPos, float margin) {
    Vector2 screen = WorldToScreen(worldPos);
    return screen.x >= -margin && screen.x <= g_screenWidth + margin &&
           screen.y >= -margin && screen.y <= g_screenHeight + margin;
}

static void UpdateGameCamera(float dt) {
    g_game.camera.target = g_game.player.pos;
    float smoothing = 5.0f;
    g_game.camera.pos.x = Lerpf(g_game.camera.pos.x, g_game.camera.target.x, smoothing * dt);
    g_game.camera.pos.y = Lerpf(g_game.camera.pos.y, g_game.camera.target.y, smoothing * dt);

    float halfW = g_screenWidth / 2.0f;
    float halfH = g_screenHeight / 2.0f;
    g_game.camera.pos.x = Clampf(g_game.camera.pos.x, WORLD_PADDING + halfW, WORLD_WIDTH - WORLD_PADDING - halfW);
    g_game.camera.pos.y = Clampf(g_game.camera.pos.y, WORLD_PADDING + halfH, WORLD_HEIGHT - WORLD_PADDING - halfH);
}

// =============================================================================
// BUFF SYSTEM
// =============================================================================

static float GetDamageMultiplier(void) {
    float mult = g_game.player.damageMultiplier;
    if (g_game.buffs[POTION_DAMAGE].active) mult *= 2.0f;

    // Apply kill combo damage bonus
    if (g_game.comboTier > COMBO_NONE) {
        mult *= (1.0f + COMBO_TIERS[g_game.comboTier].damageBonus);
    }

    // Apply crit chance and track for visual feedback
    g_lastHitWasCrit = false;
    if (g_game.player.critChance > 0 && GetRandomValue(0, 100) < (int)g_game.player.critChance) {
        mult *= 2.0f;
        g_lastHitWasCrit = true;
    }
    return mult;
}

static float GetAttackSpeedMultiplier(void) {
    return g_game.player.attackSpeedMult;
}

static float GetAreaMultiplier(void) {
    return g_game.player.areaMultiplier;
}

static int GetBonusProjectiles(void) {
    return g_game.player.bonusProjectiles;
}

static float GetSpeedMultiplier(void) {
    float mult = 1.0f;
    if (g_game.buffs[POTION_SPEED].active) mult *= 1.5f;
    return mult;
}

static float GetMagnetMultiplier(void) {
    float mult = 1.0f;
    if (g_game.buffs[POTION_MAGNET].active) mult *= 3.0f;
    return mult;
}

static bool HasShield(void) {
    return g_game.buffs[POTION_SHIELD].active;
}

static void UpdateBuffs(float dt) {
    for (int i = 0; i < POTION_COUNT; i++) {
        ActiveBuff *buff = &g_game.buffs[i];
        if (!buff->active) continue;

        buff->timer -= dt;
        if (buff->timer <= 0) {
            buff->active = false;
        }
    }
}

// Potion info for display
typedef struct {
    const char *name;
    const char *desc;
    const char *symbol;  // Single char icon
    float duration;
} PotionInfo;

static const PotionInfo POTION_INFO[POTION_COUNT] = {
    {"DAMAGE", "2x damage for 10s", "!", 10.0f},
    {"SPEED", "1.5x speed for 15s", ">", 15.0f},
    {"SHIELD", "Invincible for 5s", "*", 5.0f},
    {"MAGNET", "3x XP range for 20s", "@", 20.0f}
};

static const char *GetPotionName(PotionType type) {
    if (type >= 0 && type < POTION_COUNT) return POTION_INFO[type].name;
    return "???";
}

static const char *GetPotionDesc(PotionType type) {
    if (type >= 0 && type < POTION_COUNT) return POTION_INFO[type].desc;
    return "";
}

static const char *GetPotionSymbol(PotionType type) {
    if (type >= 0 && type < POTION_COUNT) return POTION_INFO[type].symbol;
    return "?";
}

static Color GetPotionColor(PotionType type) {
    switch (type) {
        case POTION_DAMAGE: return COLOR_POTION_DAMAGE;
        case POTION_SPEED: return COLOR_POTION_SPEED;
        case POTION_SHIELD: return COLOR_POTION_SHIELD;
        case POTION_MAGNET: return COLOR_POTION_MAGNET;
        default: return WHITE;
    }
}

static void ActivateBuff(PotionType type) {
    ActiveBuff *buff = &g_game.buffs[type];
    buff->type = type;
    buff->active = true;
    buff->duration = POTION_INFO[type].duration;
    buff->timer = buff->duration;

    // Visual feedback when activating
    Color c = GetPotionColor(type);
    g_game.screenFlash = 0.3f;
    g_game.screenFlashColor = (Color){c.r, c.g, c.b, 60};
}

// =============================================================================
// PARTICLE SYSTEM
// =============================================================================

// Track active particle count for early exit optimization
static int g_activeParticleCount = 0;

static void SpawnParticle(Vector2 pos, Vector2 vel, Color color, float size, float life) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle *p = &g_game.particles[i];
        if (!p->active) {
            p->pos = pos;
            p->vel = vel;
            p->color = color;
            p->size = size;
            p->life = life;
            p->maxLife = life;
            p->active = true;
            g_activeParticleCount++;  // Track for early exit optimization
            return;
        }
    }
}

static void SpawnParticleBurst(Vector2 pos, int count, Color color, float speed, float size) {
    for (int i = 0; i < count; i++) {
        float angle = RandomFloat(0, PI * 2);
        float spd = RandomFloat(speed * 0.5f, speed);
        Vector2 vel = {cosf(angle) * spd, sinf(angle) * spd};
        SpawnParticle(pos, vel, color, RandomFloat(size * 0.5f, size), RandomFloat(0.2f, 0.5f));
    }
}

static void UpdateParticles(float dt) {
    // OPTIMIZED: Early exit if no active particles
    if (g_activeParticleCount == 0) return;

    int activeCount = 0;
    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle *p = &g_game.particles[i];
        if (!p->active) continue;

        p->pos.x += p->vel.x * dt;
        p->pos.y += p->vel.y * dt;
        p->vel.x *= 0.95f;
        p->vel.y *= 0.95f;
        p->life -= dt;

        if (p->life <= 0) {
            p->active = false;
        } else {
            activeCount++;
        }
    }
    g_activeParticleCount = activeCount;
}

static void DrawParticles(void) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle *p = &g_game.particles[i];
        if (!p->active || !IsOnScreen(p->pos, 20)) continue;

        Vector2 screen = WorldToScreen(p->pos);
        float alpha = p->life / p->maxLife;
        Color c = p->color;
        c.a = (unsigned char)(c.a * alpha);

        float s = p->size * alpha;
        Vector2 pts[4] = {
            {screen.x, screen.y - s}, {screen.x + s, screen.y},
            {screen.x, screen.y + s}, {screen.x - s, screen.y}
        };
        DrawTriangle(pts[0], pts[1], pts[2], c);
        DrawTriangle(pts[0], pts[2], pts[3], c);
    }
}

// =============================================================================
// DYING ENEMY ANIMATIONS
// =============================================================================

static void SpawnDyingEnemy(Vector2 pos, EnemyType type, float size, Color color) {
    for (int i = 0; i < MAX_DYING_ENEMIES; i++) {
        DyingEnemy *de = &g_dyingEnemies[i];
        if (!de->active) {
            de->pos = pos;
            de->type = type;
            de->size = size;
            de->timer = DEATH_ANIM_TIME;
            de->maxTime = DEATH_ANIM_TIME;
            de->color = color;
            de->active = true;
            return;
        }
    }
}

static void UpdateDyingEnemies(float dt) {
    for (int i = 0; i < MAX_DYING_ENEMIES; i++) {
        DyingEnemy *de = &g_dyingEnemies[i];
        if (!de->active) continue;

        de->timer -= dt;
        if (de->timer <= 0) de->active = false;
    }
}

static void DrawDyingEnemies(void) {
    for (int i = 0; i < MAX_DYING_ENEMIES; i++) {
        DyingEnemy *de = &g_dyingEnemies[i];
        if (!de->active) continue;
        if (!IsOnScreen(de->pos, de->size * 2)) continue;

        Vector2 screen = WorldToScreen(de->pos);
        float progress = 1.0f - (de->timer / de->maxTime);

        // Scale down with EaseOutBack for bouncy effect
        float scale = 1.0f - EaseOutQuad(progress);
        float currentSize = de->size * scale;
        if (currentSize < 1.0f) continue;

        // Fade out
        Color color = de->color;
        color.a = (unsigned char)(255 * (1.0f - progress));

        // Rotate as it dies
        float rotation = progress * PI * 2.0f;

        float hs = currentSize / 2;
        switch (de->type) {
            case ENEMY_WALKER: {
                // Rotating square
                for (int j = 0; j < 4; j++) {
                    float a1 = rotation + j * PI / 2.0f;
                    float a2 = rotation + (j + 1) * PI / 2.0f;
                    DrawTriangle(screen,
                        (Vector2){screen.x + cosf(a1) * hs * 1.4f, screen.y + sinf(a1) * hs * 1.4f},
                        (Vector2){screen.x + cosf(a2) * hs * 1.4f, screen.y + sinf(a2) * hs * 1.4f}, color);
                }
                break;
            }
            case ENEMY_FAST: {
                // Spinning triangle
                DrawTriangle(
                    (Vector2){screen.x + cosf(rotation) * hs, screen.y + sinf(rotation) * hs},
                    (Vector2){screen.x + cosf(rotation + 2.1f) * hs, screen.y + sinf(rotation + 2.1f) * hs},
                    (Vector2){screen.x + cosf(rotation - 2.1f) * hs, screen.y + sinf(rotation - 2.1f) * hs}, color);
                break;
            }
            case ENEMY_TANK: {
                // Shrinking hexagon
                for (int j = 0; j < 6; j++) {
                    float a1 = rotation + j * PI / 3.0f;
                    float a2 = rotation + (j + 1) * PI / 3.0f;
                    DrawTriangle(screen,
                        (Vector2){screen.x + cosf(a1) * hs, screen.y + sinf(a1) * hs},
                        (Vector2){screen.x + cosf(a2) * hs, screen.y + sinf(a2) * hs}, color);
                }
                break;
            }
        }
    }
}

// =============================================================================
// SPAWN WARNING SYSTEM
// =============================================================================

static void SpawnWarningIndicator(Vector2 worldPos, EnemyType type) {
    for (int i = 0; i < MAX_SPAWN_WARNINGS; i++) {
        SpawnWarning *sw = &g_spawnWarnings[i];
        if (!sw->active) {
            sw->worldPos = worldPos;
            sw->type = type;
            sw->timer = SPAWN_WARNING_TIME;
            sw->maxTime = SPAWN_WARNING_TIME;
            sw->active = true;
            return;
        }
    }
}

static void UpdateSpawnWarnings(float dt) {
    for (int i = 0; i < MAX_SPAWN_WARNINGS; i++) {
        SpawnWarning *sw = &g_spawnWarnings[i];
        if (!sw->active) continue;

        sw->timer -= dt;
        if (sw->timer <= 0) sw->active = false;
    }
}

static void DrawSpawnWarnings(void) {
    for (int i = 0; i < MAX_SPAWN_WARNINGS; i++) {
        SpawnWarning *sw = &g_spawnWarnings[i];
        if (!sw->active) continue;

        // Calculate screen edge position
        Vector2 screenPos = WorldToScreen(sw->worldPos);

        // Clamp to screen edges with margin
        float margin = 30.0f;
        bool offScreen = false;

        if (screenPos.x < margin) { screenPos.x = margin; offScreen = true; }
        if (screenPos.x > g_screenWidth - margin) { screenPos.x = g_screenWidth - margin; offScreen = true; }
        if (screenPos.y < margin) { screenPos.y = margin; offScreen = true; }
        if (screenPos.y > g_screenHeight - margin) { screenPos.y = g_screenHeight - margin; offScreen = true; }

        if (!offScreen) continue;  // Don't show if on screen

        float progress = 1.0f - (sw->timer / sw->maxTime);
        float pulse = 0.5f + 0.5f * sinf(progress * PI * 8.0f);

        // Get enemy color
        Color color;
        switch (sw->type) {
            case ENEMY_WALKER: color = COLOR_WALKER; break;
            case ENEMY_FAST: color = COLOR_FAST; break;
            case ENEMY_TANK: color = COLOR_TANK; break;
        }
        color.a = (unsigned char)(200 * pulse);

        // Draw warning indicator (pulsing triangle pointing toward spawn)
        float size = 12.0f + 4.0f * pulse;
        Vector2 dir = Normalize((Vector2){
            sw->worldPos.x - g_game.camera.pos.x,
            sw->worldPos.y - g_game.camera.pos.y
        });
        float angle = atan2f(dir.y, dir.x);

        DrawTriangle(
            (Vector2){screenPos.x + cosf(angle) * size, screenPos.y + sinf(angle) * size},
            (Vector2){screenPos.x + cosf(angle + 2.5f) * size * 0.6f, screenPos.y + sinf(angle + 2.5f) * size * 0.6f},
            (Vector2){screenPos.x + cosf(angle - 2.5f) * size * 0.6f, screenPos.y + sinf(angle - 2.5f) * size * 0.6f},
            color);

        // Outer glow
        Color glowColor = color;
        glowColor.a = (unsigned char)(80 * pulse);
        DrawCircleV(screenPos, size + 5.0f, glowColor);
    }
}

// =============================================================================
// KILL STREAK SYSTEM
// =============================================================================

// Helper to determine combo tier from kill count
static ComboTier GetComboTierForKills(int kills) {
    // Check tiers from highest to lowest
    for (int i = COMBO_TIER_COUNT - 1; i >= 0; i--) {
        if (kills >= COMBO_TIERS[i].minKills) {
            return (ComboTier)i;
        }
    }
    return COMBO_NONE;
}

static void RegisterKill(void) {
    g_killStreak++;
    g_killStreakTimer = KILL_STREAK_TIMEOUT;

    // Update kill combo system
    g_game.killCombo++;
    g_game.killComboTimer = KILL_COMBO_TIMEOUT;

    // Track highest combo this run
    if (g_game.killCombo > g_game.highestCombo) {
        g_game.highestCombo = g_game.killCombo;
    }

    // Check for tier change
    g_game.prevComboTier = g_game.comboTier;
    g_game.comboTier = GetComboTierForKills(g_game.killCombo);

    // Trigger flash and celebration when tier increases
    if (g_game.comboTier > g_game.prevComboTier && g_game.comboTier != COMBO_NONE) {
        g_game.comboTierFlash = 1.5f;  // 1.5 second flash

        // Celebration effects scale with tier
        float intensity = (float)g_game.comboTier / (float)(COMBO_TIER_COUNT - 1);
        g_game.screenFlash = 0.2f + intensity * 0.3f;
        g_game.screenFlashColor = COMBO_TIERS[g_game.comboTier].color;
        g_game.screenShake = 0.1f + intensity * 0.2f;

        // Particle burst in tier color
        int particleCount = 8 + g_game.comboTier * 4;
        for (int j = 0; j < particleCount; j++) {
            float angle = (float)j / (float)particleCount * PI * 2.0f;
            float speed = 100.0f + intensity * 100.0f + RandomFloat(0, 40);
            Vector2 vel = {cosf(angle) * speed, sinf(angle) * speed};
            SpawnParticle(g_game.player.pos, vel, COMBO_TIERS[g_game.comboTier].color, RandomFloat(3, 6), 0.6f);
        }
    }

    // Check for kill streak milestone (existing system)
    for (int i = NUM_KILL_MILESTONES - 1; i >= 0; i--) {
        if (g_killStreak == KILL_MILESTONES[i]) {
            g_killStreakMilestone = i;
            g_killStreakDisplay = KILL_STREAK_DISPLAY_TIME;

            // Celebration effects
            g_game.screenFlash = 0.4f;
            g_game.screenFlashColor = (Color){255, 200, 50, 100};
            g_game.screenShake = 0.25f;

            // Radial burst from player
            for (int j = 0; j < 16; j++) {
                float angle = (float)j / 16.0f * PI * 2.0f;
                float speed = 150.0f + RandomFloat(0, 50);
                Vector2 vel = {cosf(angle) * speed, sinf(angle) * speed};
                SpawnParticle(g_game.player.pos, vel, (Color){255, 215, 0, 255}, RandomFloat(4, 7), 0.5f);
            }
            break;
        }
    }
}

static void UpdateKillStreak(float dt) {
    if (g_killStreakTimer > 0) {
        g_killStreakTimer -= dt;
        if (g_killStreakTimer <= 0) {
            g_killStreak = 0;  // Reset streak
        }
    }

    if (g_killStreakDisplay > 0) {
        g_killStreakDisplay -= dt;
    }

    // Update kill combo timer
    if (g_game.killComboTimer > 0) {
        g_game.killComboTimer -= dt;
        if (g_game.killComboTimer <= 0) {
            // Combo expired - reset
            g_game.killCombo = 0;
            g_game.comboTier = COMBO_NONE;
            g_game.prevComboTier = COMBO_NONE;
        }
    }

    // Update combo tier flash
    if (g_game.comboTierFlash > 0) {
        g_game.comboTierFlash -= dt;
    }
}

static void DrawKillStreakAnnouncement(void) {
    if (g_killStreakDisplay <= 0) return;

    float alpha = Clampf(g_killStreakDisplay / 0.3f, 0, 1);  // Fade out in last 0.3s
    float progress = 1.0f - (g_killStreakDisplay / KILL_STREAK_DISPLAY_TIME);
    float scale = EaseOutBack(fminf(progress * 3.0f, 1.0f));  // Pop in effect

    const char *text = KILL_MILESTONE_NAMES[g_killStreakMilestone];
    float fontSize = 36 * scale;
    int textWidth = (int)MeasureTextEx(g_font, text, fontSize, 1).x;

    Color textColor = {255, 215, 0, (unsigned char)(255 * alpha)};
    Color shadowColor = {0, 0, 0, (unsigned char)(180 * alpha)};

    float x = g_screenWidth / 2.0f - textWidth / 2.0f;
    float y = 120.0f;

    // Shadow
    DrawTextEx(g_font, text, (Vector2){x + 2, y + 2}, fontSize, 1, shadowColor);
    // Main text
    DrawTextEx(g_font, text, (Vector2){x, y}, fontSize, 1, textColor);

    // Streak count below
    char countText[32];
    snprintf(countText, sizeof(countText), "%d KILLS", g_killStreak);
    float countFontSize = 18 * scale;
    int countWidth = (int)MeasureTextEx(g_font, countText, countFontSize, 1).x;
    Color countColor = {255, 255, 255, (unsigned char)(200 * alpha)};
    DrawTextEx(g_font, countText, (Vector2){g_screenWidth / 2.0f - countWidth / 2.0f, y + fontSize + 5}, countFontSize, 1, countColor);
}

// Draw combo tier announcement when tier changes
static void DrawComboTierAnnouncement(void) {
    if (g_game.comboTierFlash <= 0 || g_game.comboTier == COMBO_NONE) return;

    float alpha = Clampf(g_game.comboTierFlash / 0.5f, 0, 1);  // Fade out in last 0.5s
    float progress = 1.0f - (g_game.comboTierFlash / 1.5f);
    float scale = EaseOutBack(fminf(progress * 4.0f, 1.0f));  // Pop in effect

    const char *tierName = COMBO_TIERS[g_game.comboTier].name;
    float fontSize = 32 * scale;
    int textWidth = (int)MeasureTextEx(g_font, tierName, fontSize, 1).x;

    Color tierColor = COMBO_TIERS[g_game.comboTier].color;
    tierColor.a = (unsigned char)(255 * alpha);
    Color shadowColor = {0, 0, 0, (unsigned char)(180 * alpha)};

    // Position above center screen
    float x = g_screenWidth / 2.0f - textWidth / 2.0f;
    float y = 85.0f;

    // Shadow
    DrawTextEx(g_font, tierName, (Vector2){x + 2, y + 2}, fontSize, 1, shadowColor);
    // Main text
    DrawTextEx(g_font, tierName, (Vector2){x, y}, fontSize, 1, tierColor);
}

// Draw combo meter in bottom left
static void DrawComboMeter(void) {
    if (g_game.killCombo < 5) return;  // Only show when combo is active

    int meterX = 10;
    int meterY = g_screenHeight - 130;  // Position above existing streak display

    Color tierColor = COMBO_TIERS[g_game.comboTier].color;
    const char *tierName = COMBO_TIERS[g_game.comboTier].name;

    // Pulsing effect based on tier
    float pulse = 0.8f + 0.2f * sinf(g_game.bgTime * (3.0f + g_game.comboTier));

    // Combo count with tier color
    char comboText[32];
    snprintf(comboText, sizeof(comboText), "%d COMBO", g_game.killCombo);
    float fontSize = 16 * pulse;
    Font comboFont = LlzFontGet(LLZ_FONT_UI, (int)fontSize);
    DrawTextEx(comboFont, comboText, (Vector2){meterX, meterY}, fontSize, 1, tierColor);

    // Tier name below (if not NONE)
    if (g_game.comboTier > COMBO_NONE) {
        fontSize = 12;
        DrawTextEx(g_font, tierName, (Vector2){meterX, meterY + 18}, fontSize, 1, tierColor);
    }

    // Timer bar showing time remaining
    float timerPercent = g_game.killComboTimer / KILL_COMBO_TIMEOUT;
    int barWidth = 80;
    int barHeight = 4;
    int barY = meterY + 32;

    // Background
    DrawRectangle(meterX, barY, barWidth, barHeight, (Color){30, 30, 40, 200});
    // Fill
    Color barColor = tierColor;
    barColor.a = 200;
    DrawRectangle(meterX, barY, (int)(barWidth * timerPercent), barHeight, barColor);

    // Bonus indicators
    char bonusText[32];
    Color bonusColor = {150, 255, 150, 200};
    float xpBonus = COMBO_TIERS[g_game.comboTier].xpBonus;
    float dmgBonus = COMBO_TIERS[g_game.comboTier].damageBonus;
    snprintf(bonusText, sizeof(bonusText), "XP x%.1f  DMG +%d%%", xpBonus, (int)(dmgBonus * 100));
    DrawTextEx(g_font, bonusText, (Vector2){meterX, barY + 6}, 10, 1, bonusColor);
}

// =============================================================================
// KILL MILESTONE REWARDS SYSTEM
// =============================================================================

// Milestone thresholds and rewards
static const int MILESTONE_KILLS[] = {50, 100, 250, 500, 750, 1000, 1500, 2000};
static const MilestoneReward MILESTONE_REWARDS[] = {
    MILESTONE_HEAL,           // 50 kills: +30 HP
    MILESTONE_UPGRADE_POINT,  // 100 kills: +1 upgrade point
    MILESTONE_DAMAGE_BUFF,    // 250 kills: +5% permanent damage
    MILESTONE_SPEED_BUFF,     // 500 kills: +3% permanent speed
    MILESTONE_MAGNET_PULSE,   // 750 kills: vacuum all XP
    MILESTONE_NUKE,           // 1000 kills: damage all enemies
    MILESTONE_UPGRADE_POINT,  // 1500 kills: +1 upgrade point
    MILESTONE_DAMAGE_BUFF,    // 2000 kills: +5% permanent damage
};
static const char *MILESTONE_NAMES[] = {
    "FIRST BLOOD!", "CENTURION!", "QUARTER THOUSAND!",
    "HALFWAY THERE!", "LEGEND!", "EXTERMINATOR!",
    "GODSLAYER!", "ULTIMATE SURVIVOR!"
};
static const char *MILESTONE_DESCRIPTIONS[] = {
    "+30 HP", "+1 Upgrade Point", "+5% Damage",
    "+3% Speed", "XP Magnet Pulse!", "NUKE!",
    "+1 Upgrade Point", "+5% Damage"
};

#define MILESTONE_CELEBRATION_TIME 2.0f

static void InitMilestones(void) {
    for (int i = 0; i < MAX_MILESTONES; i++) {
        g_game.milestones[i].killThreshold = MILESTONE_KILLS[i];
        g_game.milestones[i].reward = MILESTONE_REWARDS[i];
        g_game.milestones[i].name = MILESTONE_NAMES[i];
        g_game.milestones[i].description = MILESTONE_DESCRIPTIONS[i];
        g_game.milestones[i].claimed = false;
    }
    g_game.nextMilestoneIdx = 0;
    g_game.milestoneFlash = 0;
    g_game.milestoneCelebrationTimer = 0;
}

static void AwardMilestoneReward(MilestoneReward reward) {
    Player *p = &g_game.player;

    switch (reward) {
        case MILESTONE_HEAL:
            p->hp += 30;
            if (p->hp > p->maxHp) p->hp = p->maxHp;
            break;

        case MILESTONE_UPGRADE_POINT:
            p->upgradePoints++;
            break;

        case MILESTONE_DAMAGE_BUFF:
            p->damageMultiplier += 0.05f;  // +5% permanent damage
            break;

        case MILESTONE_SPEED_BUFF:
            p->speed *= 1.03f;  // +3% permanent speed
            break;

        case MILESTONE_MAGNET_PULSE:
            // Magnetize all XP gems on screen
            for (int i = 0; i < MAX_XP_GEMS; i++) {
                if (g_game.xpGems[i].active) {
                    g_game.xpGems[i].magnetized = true;
                }
            }
            break;

        case MILESTONE_NUKE:
            // Damage all enemies on screen
            for (int i = 0; i < MAX_ENEMIES; i++) {
                Enemy *e = &g_game.enemies[i];
                if (e->active && !e->isDecoy) {
                    // Deal 50% of enemy max HP as damage
                    int nukeDamage = e->maxHp / 2;
                    if (nukeDamage < 10) nukeDamage = 10;
                    e->hp -= nukeDamage;
                    e->hitFlash = 0.3f;
                    SpawnParticleBurst(e->pos, 6, (Color){255, 100, 50, 255}, 60, 4);
                }
            }
            g_game.screenShake = 0.5f;
            break;
    }
}

static void CheckMilestones(void) {
    if (g_game.nextMilestoneIdx >= MAX_MILESTONES) return;

    KillMilestone *m = &g_game.milestones[g_game.nextMilestoneIdx];

    if (g_game.killCount >= m->killThreshold && !m->claimed) {
        // Award the reward
        AwardMilestoneReward(m->reward);
        m->claimed = true;

        // Start celebration
        g_game.milestoneCelebrationTimer = MILESTONE_CELEBRATION_TIME;
        g_game.milestoneFlash = 1.0f;

        // Visual effects
        g_game.screenFlash = 0.5f;
        g_game.screenFlashColor = (Color){255, 215, 0, 150};
        g_game.screenShake = 0.3f;

        // Big particle burst from player
        for (int j = 0; j < 24; j++) {
            float angle = (float)j / 24.0f * PI * 2.0f;
            float speed = 200.0f + RandomFloat(0, 80);
            Vector2 vel = {cosf(angle) * speed, sinf(angle) * speed};
            Color c = (j % 2 == 0) ? (Color){255, 215, 0, 255} : (Color){255, 255, 255, 255};
            SpawnParticle(g_game.player.pos, vel, c, RandomFloat(5, 9), 0.7f);
        }

        // Move to next milestone
        g_game.nextMilestoneIdx++;
    }
}

static void UpdateMilestones(float dt) {
    if (g_game.milestoneCelebrationTimer > 0) {
        g_game.milestoneCelebrationTimer -= dt;
    }
    if (g_game.milestoneFlash > 0) {
        g_game.milestoneFlash -= dt * 2.0f;
    }
}

static void DrawMilestoneProgressHUD(void) {
    // Don't draw if all milestones claimed
    if (g_game.nextMilestoneIdx >= MAX_MILESTONES) return;

    KillMilestone *m = &g_game.milestones[g_game.nextMilestoneIdx];

    // Calculate progress toward next milestone
    int prevKills = (g_game.nextMilestoneIdx > 0)
        ? g_game.milestones[g_game.nextMilestoneIdx - 1].killThreshold
        : 0;
    int targetKills = m->killThreshold;
    float progress = (float)(g_game.killCount - prevKills) / (float)(targetKills - prevKills);
    if (progress < 0) progress = 0;
    if (progress > 1) progress = 1;

    // Draw small progress bar in top-right area (below minimap)
    int barX = g_screenWidth - MINIMAP_WIDTH - 10;
    int barY = MINIMAP_Y + MINIMAP_HEIGHT + 10;
    int barW = MINIMAP_WIDTH;
    int barH = 8;

    // Background
    DrawRectangle(barX, barY, barW, barH, (Color){30, 30, 40, 200});

    // Progress fill with pulsing glow when close
    Color fillColor = (Color){255, 180, 50, 255};
    if (progress > 0.8f) {
        float pulse = 0.7f + 0.3f * sinf(g_game.bgTime * 6.0f);
        fillColor.a = (unsigned char)(255 * pulse);
    }
    DrawRectangle(barX, barY, (int)(barW * progress), barH, fillColor);

    // Border
    DrawRectangleLines(barX, barY, barW, barH, (Color){100, 100, 120, 255});

    // Label showing kills / target
    char buf[32];
    snprintf(buf, sizeof(buf), "%d/%d", g_game.killCount, targetKills);
    int tw = (int)MeasureTextEx(g_font, buf, 10, 1).x;
    DrawTextEx(g_font, buf, (Vector2){barX + barW / 2 - tw / 2, barY + barH + 2}, 10, 1, COLOR_TEXT_DIM);

    // Reward preview
    DrawTextEx(g_font, m->description, (Vector2){barX, barY - 12}, 10, 1, (Color){255, 200, 100, 200});
}

static void DrawMilestoneCelebration(void) {
    if (g_game.milestoneCelebrationTimer <= 0) return;
    if (g_game.nextMilestoneIdx <= 0) return;

    // Get the milestone that was just claimed
    KillMilestone *m = &g_game.milestones[g_game.nextMilestoneIdx - 1];

    float progress = 1.0f - (g_game.milestoneCelebrationTimer / MILESTONE_CELEBRATION_TIME);
    float alpha = 1.0f - progress;  // Fade out over time
    if (alpha < 0) alpha = 0;

    // Scale in with bounce
    float scale = EaseOutBack(fminf(progress * 3.0f, 1.0f));

    // Draw big announcement at center
    float fontSize = 40 * scale;
    int nameWidth = (int)MeasureTextEx(g_font, m->name, fontSize, 1).x;

    float x = g_screenWidth / 2.0f - nameWidth / 2.0f;
    float y = g_screenHeight / 2.0f - 60.0f;

    // Glow background
    Color glowColor = {255, 200, 50, (unsigned char)(100 * alpha)};
    DrawCircleGradient((int)(g_screenWidth / 2), (int)y + 20, 200 * scale, glowColor, BLANK);

    // Shadow
    Color shadowColor = {0, 0, 0, (unsigned char)(180 * alpha)};
    DrawTextEx(g_font, m->name, (Vector2){x + 3, y + 3}, fontSize, 1, shadowColor);

    // Main text (golden)
    Color textColor = {255, 215, 0, (unsigned char)(255 * alpha)};
    DrawTextEx(g_font, m->name, (Vector2){x, y}, fontSize, 1, textColor);

    // Description below
    float descFontSize = 22 * scale;
    int descWidth = (int)MeasureTextEx(g_font, m->description, descFontSize, 1).x;
    Color descColor = {255, 255, 255, (unsigned char)(220 * alpha)};
    DrawTextEx(g_font, m->description,
               (Vector2){g_screenWidth / 2.0f - descWidth / 2.0f, y + fontSize + 10},
               descFontSize, 1, descColor);

    // Kill count
    char killBuf[32];
    snprintf(killBuf, sizeof(killBuf), "%d KILLS", m->killThreshold);
    float killFontSize = 16 * scale;
    int killWidth = (int)MeasureTextEx(g_font, killBuf, killFontSize, 1).x;
    Color killColor = {200, 200, 220, (unsigned char)(180 * alpha)};
    DrawTextEx(g_font, killBuf,
               (Vector2){g_screenWidth / 2.0f - killWidth / 2.0f, y + fontSize + descFontSize + 20},
               killFontSize, 1, killColor);
}

// =============================================================================
// WAVE CELEBRATION
// =============================================================================

static void TriggerWaveCelebration(int newWave) {
    g_waveCelebration = WAVE_CELEBRATION_TIME;
    g_lastWave = newWave;

    // Screen effects
    g_game.screenFlash = 0.3f;
    g_game.screenFlashColor = (Color){100, 200, 255, 80};

    // Spawn celebration particles around screen edges
    for (int i = 0; i < 20; i++) {
        float x = RandomFloat(0, WORLD_WIDTH);
        float y = RandomFloat(0, WORLD_HEIGHT);
        // Keep near player
        x = g_game.player.pos.x + RandomFloat(-300, 300);
        y = g_game.player.pos.y + RandomFloat(-200, 200);

        Vector2 vel = {RandomFloat(-30, 30), RandomFloat(-60, -30)};
        Color c = (i % 2 == 0) ? COLOR_XP_BAR : (Color){255, 255, 255, 255};
        SpawnParticle((Vector2){x, y}, vel, c, RandomFloat(3, 6), RandomFloat(0.5f, 1.0f));
    }
}

static void DrawWaveCelebration(void) {
    if (g_waveCelebration <= 0) return;

    float alpha = Clampf(g_waveCelebration / 0.5f, 0, 1);
    float progress = 1.0f - (g_waveCelebration / WAVE_CELEBRATION_TIME);
    float scale = EaseOutElastic(fminf(progress * 2.0f, 1.0f));

    char text[32];
    snprintf(text, sizeof(text), "WAVE %d", g_lastWave + 1);
    float fontSize = 42 * scale;
    int textWidth = (int)MeasureTextEx(g_font, text, fontSize, 1).x;

    Color textColor = {100, 200, 255, (unsigned char)(255 * alpha)};
    Color shadowColor = {0, 0, 0, (unsigned char)(180 * alpha)};

    float x = g_screenWidth / 2.0f - textWidth / 2.0f;
    float y = 80.0f;

    DrawTextEx(g_font, text, (Vector2){x + 2, y + 2}, fontSize, 1, shadowColor);
    DrawTextEx(g_font, text, (Vector2){x, y}, fontSize, 1, textColor);
}

// =============================================================================
// ENEMY INTRODUCTION SYSTEM
// =============================================================================

static Color GetEnemyColor(EnemyType type) {
    switch (type) {
        case ENEMY_WALKER: return COLOR_WALKER;
        case ENEMY_FAST: return COLOR_FAST;
        case ENEMY_TANK: return COLOR_TANK;
        case ENEMY_SWARM: return COLOR_SWARM;
        case ENEMY_ELITE: return COLOR_ELITE;
        case ENEMY_HORNET: return COLOR_HORNET;
        case ENEMY_BRUTE: return COLOR_BRUTE;
        case ENEMY_SPINNER: return COLOR_SPINNER;
        case ENEMY_MIRROR: return COLOR_MIRROR;
        case ENEMY_SHIELDER: return COLOR_SHIELDER;
        case ENEMY_BOSS: return COLOR_BOSS;
        case ENEMY_BOMBER: return COLOR_BOMBER;
        case ENEMY_PHASER: return COLOR_PHASER;
        default: return WHITE;
    }
}

static void UnlockEnemy(EnemyType type) {
    if (type >= ENEMY_TYPE_COUNT) return;
    if (g_enemyPoolUnlocked[type]) return;  // Already unlocked

    g_enemyPoolUnlocked[type] = true;
    g_enemyIntroType = type;
    g_enemyIntroTimer = ENEMY_INTRO_TIME;
    g_enemyIntroActive = true;

    // Big celebration for new enemy unlock
    g_game.screenFlash = 0.5f;
    Color enemyColor = GetEnemyColor(type);
    g_game.screenFlashColor = (Color){enemyColor.r, enemyColor.g, enemyColor.b, 100};
    g_game.screenShake = 0.3f;

    // Particles in enemy color
    for (int i = 0; i < 20; i++) {
        float angle = RandomFloat(0, PI * 2);
        float speed = 100.0f + RandomFloat(0, 100);
        Vector2 vel = {cosf(angle) * speed, sinf(angle) * speed};
        Vector2 pos = {g_screenWidth / 2.0f + g_game.camera.pos.x - g_screenWidth / 2.0f,
                       120.0f + g_game.camera.pos.y - g_screenHeight / 2.0f};
        SpawnParticle(pos, vel, enemyColor, RandomFloat(4, 8), 0.8f);
    }
}

static void CheckWaveUnlocks(int wave) {
    for (int i = 0; i < ENEMY_TYPE_COUNT; i++) {
        if (!g_enemyPoolUnlocked[i] && wave >= ENEMY_UNLOCK_WAVES[i]) {
            UnlockEnemy((EnemyType)i);
            break;  // Only unlock one per wave transition
        }
    }
}

static void DrawEnemyIntroduction(void) {
    if (!g_enemyIntroActive || g_enemyIntroTimer <= 0) return;

    float progress = 1.0f - (g_enemyIntroTimer / ENEMY_INTRO_TIME);
    float alpha;

    // Fade in for first 0.3s, hold, fade out for last 0.5s
    if (progress < 0.1f) {
        alpha = progress / 0.1f;
    } else if (progress > 0.8f) {
        alpha = (1.0f - progress) / 0.2f;
    } else {
        alpha = 1.0f;
    }

    // Pop-in scale effect
    float scale = EaseOutBack(fminf(progress * 5.0f, 1.0f));

    Color enemyColor = GetEnemyColor(g_enemyIntroType);
    enemyColor.a = (unsigned char)(255 * alpha);

    // Dark overlay
    DrawRectangle(0, 50, g_screenWidth, 120, (Color){0, 0, 0, (unsigned char)(180 * alpha)});

    // "NEW THREAT" header
    const char *header = "NEW THREAT DETECTED";
    float headerSize = 18 * scale;
    int headerWidth = (int)MeasureTextEx(g_font, header, headerSize, 1).x;
    Color headerColor = {255, 100, 100, (unsigned char)(255 * alpha)};
    DrawTextEx(g_font, header, (Vector2){g_screenWidth / 2.0f - headerWidth / 2.0f, 60}, headerSize, 1, headerColor);

    // Enemy name (big)
    const char *name = ENEMY_NAMES[g_enemyIntroType];
    float nameSize = 36 * scale;
    int nameWidth = (int)MeasureTextEx(g_font, name, nameSize, 1).x;

    // Glow behind name
    Color glowColor = enemyColor;
    glowColor.a = (unsigned char)(60 * alpha);
    DrawRectangle((int)(g_screenWidth / 2.0f - nameWidth / 2.0f - 20), 85,
                  nameWidth + 40, (int)(nameSize + 10), glowColor);

    Color shadowColor = {0, 0, 0, (unsigned char)(200 * alpha)};
    DrawTextEx(g_font, name, (Vector2){g_screenWidth / 2.0f - nameWidth / 2.0f + 2, 90 + 2}, nameSize, 1, shadowColor);
    DrawTextEx(g_font, name, (Vector2){g_screenWidth / 2.0f - nameWidth / 2.0f, 90}, nameSize, 1, enemyColor);

    // Description
    const char *desc = ENEMY_DESCRIPTIONS[g_enemyIntroType];
    float descSize = 14 * scale;
    int descWidth = (int)MeasureTextEx(g_font, desc, descSize, 1).x;
    Color descColor = {200, 200, 200, (unsigned char)(200 * alpha)};
    DrawTextEx(g_font, desc, (Vector2){g_screenWidth / 2.0f - descWidth / 2.0f, 135}, descSize, 1, descColor);

    // Pulsing border
    float pulse = 0.5f + 0.5f * sinf(progress * PI * 8.0f);
    Color borderColor = enemyColor;
    borderColor.a = (unsigned char)(150 * alpha * pulse);
    DrawRectangleLinesEx((Rectangle){10, 55, g_screenWidth - 20, 110}, 3, borderColor);
}

// =============================================================================
// TEXT POPUPS (XP collection feedback)
// =============================================================================

static void SpawnTextPopup(Vector2 pos, const char *text, Color color, float scale) {
    for (int i = 0; i < MAX_POPUPS; i++) {
        TextPopup *p = &g_game.popups[i];
        if (!p->active) {
            p->pos = pos;
            p->vel = (Vector2){RandomFloat(-20, 20), -80};
            strncpy(p->text, text, sizeof(p->text) - 1);
            p->text[sizeof(p->text) - 1] = '\0';
            p->color = color;
            p->life = 0.8f;
            p->maxLife = 0.8f;
            p->scale = scale;
            p->active = true;
            return;
        }
    }
}

static void UpdateTextPopups(float dt) {
    for (int i = 0; i < MAX_POPUPS; i++) {
        TextPopup *p = &g_game.popups[i];
        if (!p->active) continue;

        p->pos.x += p->vel.x * dt;
        p->pos.y += p->vel.y * dt;
        p->vel.y += 50 * dt;
        p->life -= dt;

        if (p->life <= 0) p->active = false;
    }
}

static void DrawTextPopups(void) {
    for (int i = 0; i < MAX_POPUPS; i++) {
        TextPopup *p = &g_game.popups[i];
        if (!p->active) continue;

        Vector2 screen = WorldToScreen(p->pos);
        if (screen.x < -50 || screen.x > g_screenWidth + 50) continue;

        float alpha = p->life / p->maxLife;
        float scale = p->scale * (1.0f + (1.0f - alpha) * 0.3f);

        Color c = p->color;
        c.a = (unsigned char)(255 * alpha);

        Color shadow = {0, 0, 0, (unsigned char)(150 * alpha)};
        float fontSize = 16 * scale;
        int tw = (int)MeasureTextEx(g_font, p->text, fontSize, 1).x;

        DrawTextEx(g_font, p->text, (Vector2){screen.x - tw/2 + 1, screen.y + 1}, fontSize, 1, shadow);
        DrawTextEx(g_font, p->text, (Vector2){screen.x - tw/2, screen.y}, fontSize, 1, c);
    }
}

// =============================================================================
// UI PARTICLES (fly to HUD)
// =============================================================================

static void SpawnUIParticle(Vector2 worldPos, Color color) {
    Vector2 screen = WorldToScreen(worldPos);
    Vector2 target = {80, 34};  // XP bar center

    for (int i = 0; i < MAX_UI_PARTICLES; i++) {
        UIParticle *p = &g_game.uiParticles[i];
        if (!p->active) {
            p->pos = screen;
            p->target = target;
            p->color = color;
            p->life = 0.5f;
            p->speed = 400 + RandomFloat(0, 200);
            p->active = true;
            return;
        }
    }
}

static void UpdateUIParticles(float dt) {
    for (int i = 0; i < MAX_UI_PARTICLES; i++) {
        UIParticle *p = &g_game.uiParticles[i];
        if (!p->active) continue;

        Vector2 dir = Normalize((Vector2){p->target.x - p->pos.x, p->target.y - p->pos.y});
        p->pos.x += dir.x * p->speed * dt;
        p->pos.y += dir.y * p->speed * dt;
        p->life -= dt;

        float dist = Distance(p->pos, p->target);
        if (dist < 10 || p->life <= 0) {
            p->active = false;
            g_game.xpBarPulse = fmaxf(g_game.xpBarPulse, 0.5f);
        }
    }
}

static void DrawUIParticles(void) {
    for (int i = 0; i < MAX_UI_PARTICLES; i++) {
        UIParticle *p = &g_game.uiParticles[i];
        if (!p->active) continue;

        float alpha = p->life / 0.5f;
        Color c = p->color;
        c.a = (unsigned char)(255 * alpha);

        DrawCircleV(p->pos, 3, c);

        Vector2 dir = Normalize((Vector2){p->target.x - p->pos.x, p->target.y - p->pos.y});
        Vector2 tail = {p->pos.x - dir.x * 8, p->pos.y - dir.y * 8};
        c.a = (unsigned char)(100 * alpha);
        DrawLineEx(tail, p->pos, 2, c);
    }
}

// =============================================================================
// POTIONS
// =============================================================================

static void SpawnPotion(Vector2 pos) {
    for (int i = 0; i < MAX_POTIONS; i++) {
        Potion *p = &g_game.potions[i];
        if (!p->active) {
            p->pos = pos;
            p->vel = (Vector2){RandomFloat(-40, 40), RandomFloat(-40, 40)};
            p->type = (PotionType)GetRandomValue(0, POTION_COUNT - 1);
            p->active = true;
            p->bobTimer = RandomFloat(0, PI * 2);
            return;
        }
    }
}

static bool AddToInventory(PotionType type) {
    for (int i = 0; i < MAX_INVENTORY_POTIONS; i++) {
        if (!g_game.inventory[i].active) {
            g_game.inventory[i].type = type;
            g_game.inventory[i].active = true;
            return true;
        }
    }
    return false;
}

static void UpdatePotions(float dt) {
    Player *player = &g_game.player;

    for (int i = 0; i < MAX_POTIONS; i++) {
        Potion *p = &g_game.potions[i];
        if (!p->active) continue;

        p->bobTimer += dt * 3.0f;
        p->pos.x += p->vel.x * dt;
        p->pos.y += p->vel.y * dt;
        p->vel.x *= 0.97f;
        p->vel.y *= 0.97f;

        float dist = Distance(p->pos, player->pos);
        if (dist < PLAYER_PICKUP_RANGE + 10) {
            if (AddToInventory(p->type)) {
                p->active = false;
                SpawnParticleBurst(p->pos, 4, COLOR_XP_MEDIUM, 60, 3);
            }
        }
    }
}

static void DrawPotions(void) {
    for (int i = 0; i < MAX_POTIONS; i++) {
        Potion *p = &g_game.potions[i];
        if (!p->active || !IsOnScreen(p->pos, 20)) continue;

        Vector2 screen = WorldToScreen(p->pos);
        float bob = sinf(p->bobTimer) * 2.0f;
        Color color = GetPotionColor(p->type);

        // Draw potion bottle shape with glow
        DrawCircleV((Vector2){screen.x, screen.y + bob - 2}, 10, (Color){color.r, color.g, color.b, 60});
        DrawCircleV((Vector2){screen.x, screen.y + bob - 2}, 7, color);
        DrawRectangle((int)screen.x - 4, (int)(screen.y + bob - 10), 8, 8, color);
        DrawRectangle((int)screen.x - 3, (int)(screen.y + bob - 13), 6, 4, WHITE);

        // Draw symbol on bottle
        const char *symbol = GetPotionSymbol(p->type);
        DrawTextEx(g_font, symbol, (Vector2){screen.x - 3, screen.y + bob - 6}, 10, 0, WHITE);
    }
}

// =============================================================================
// XP GEMS
// =============================================================================

static void SpawnXPGem(Vector2 pos, int xpValue) {
    for (int i = 0; i < MAX_XP_GEMS; i++) {
        XPGem *gem = &g_game.xpGems[i];
        if (!gem->active) {
            gem->pos = pos;
            gem->vel = (Vector2){RandomFloat(-30, 30), RandomFloat(-30, 30)};
            gem->active = true;
            gem->bobTimer = RandomFloat(0, PI * 2);
            gem->magnetized = false;
            gem->glowTimer = RandomFloat(0, PI * 2);
            gem->sparkleTimer = RandomFloat(0, PI * 2);

            if (xpValue >= 30) { gem->type = XP_LARGE; gem->value = 40; }
            else if (xpValue >= 12) { gem->type = XP_MEDIUM; gem->value = 15; }
            else { gem->type = XP_SMALL; gem->value = 5; }
            return;
        }
    }
}

static void UpdateXPGems(float dt) {
    Player *player = &g_game.player;
    float magnetRange = player->magnetRange * GetMagnetMultiplier();

    for (int i = 0; i < MAX_XP_GEMS; i++) {
        XPGem *gem = &g_game.xpGems[i];
        if (!gem->active) continue;

        gem->bobTimer += dt * 4.0f;
        gem->glowTimer += dt;
        gem->sparkleTimer += dt;
        float dist = Distance(gem->pos, player->pos);

        if (dist < magnetRange) gem->magnetized = true;

        if (gem->magnetized) {
            Vector2 dir = Normalize((Vector2){player->pos.x - gem->pos.x, player->pos.y - gem->pos.y});

            // Homing acceleration: speed increases as gem gets closer
            // Start at base speed, accelerate up to 3x as it approaches
            float distFactor = 1.0f - Clampf(dist / magnetRange, 0.0f, 1.0f);  // 0 at edge, 1 at player
            float accelMult = 1.0f + distFactor * 2.0f;  // 1x to 3x speed

            // Apply acceleration to velocity (for smooth acceleration feel)
            float targetSpeed = XP_GEM_MAGNET_SPEED * accelMult;
            gem->vel.x = Lerpf(gem->vel.x, dir.x * targetSpeed, dt * 8.0f);
            gem->vel.y = Lerpf(gem->vel.y, dir.y * targetSpeed, dt * 8.0f);

            gem->pos.x += gem->vel.x * dt;
            gem->pos.y += gem->vel.y * dt;

            // More trail particles when moving fast
            int trailChance = 15 + (int)(distFactor * 30);  // 15-45% chance
            if (GetRandomValue(0, 100) < trailChance) {
                Color trailColor;
                switch (gem->type) {
                    case XP_LARGE: trailColor = COLOR_XP_LARGE; break;
                    case XP_MEDIUM: trailColor = COLOR_XP_MEDIUM; break;
                    default: trailColor = COLOR_XP_SMALL; break;
                }
                trailColor.a = (unsigned char)(100 + distFactor * 155);  // Brighter when faster
                float trailSize = 2.0f + distFactor * 2.0f;
                SpawnParticle(gem->pos, (Vector2){RandomFloat(-15, 15), RandomFloat(-15, 15)}, trailColor, trailSize, 0.2f);
            }
        } else {
            gem->pos.x += gem->vel.x * dt;
            gem->pos.y += gem->vel.y * dt;
            gem->vel.x *= 0.98f;
            gem->vel.y *= 0.98f;
        }

        if (dist < PLAYER_PICKUP_RANGE) {
            // Apply kill streak XP multiplier (caps at 3x) and class XP multiplier
            float streakMult = 1.0f + (g_killStreak / 10.0f);
            if (streakMult > 3.0f) streakMult = 3.0f;
            float totalMult = streakMult * player->xpMultiplier;  // Apply class XP bonus

            // Apply kill combo tier XP bonus
            if (g_game.comboTier > COMBO_NONE) {
                totalMult *= COMBO_TIERS[g_game.comboTier].xpBonus;
            }

            int xpGain = (int)(gem->value * totalMult);
            player->xp += xpGain;

            // Update combo system
            if (g_game.comboTimer > 0) {
                g_game.xpCombo++;
            } else {
                g_game.xpCombo = 1;
            }
            g_game.comboTimer = 0.5f;

            // Spawn XP popup
            char popupText[16];
            Color popupColor;
            float popupScale = 1.0f;

            if (streakMult > 1.0f && g_game.xpCombo > 5) {
                snprintf(popupText, sizeof(popupText), "+%d x%.1f!", xpGain, streakMult);
                popupColor = (Color){255, 215, 0, 255};  // Gold for streak bonus
                popupScale = 1.4f;
            } else if (g_game.xpCombo > 5) {
                snprintf(popupText, sizeof(popupText), "+%d x%d!", xpGain, g_game.xpCombo);
                popupColor = COLOR_XP_LARGE;
                popupScale = 1.3f;
            } else if (g_game.xpCombo > 1) {
                snprintf(popupText, sizeof(popupText), "+%d x%d", xpGain, g_game.xpCombo);
                popupColor = COLOR_XP_MEDIUM;
                popupScale = 1.1f;
            } else {
                snprintf(popupText, sizeof(popupText), "+%d", xpGain);
                switch (gem->type) {
                    case XP_LARGE: popupColor = COLOR_XP_LARGE; break;
                    case XP_MEDIUM: popupColor = COLOR_XP_MEDIUM; break;
                    default: popupColor = COLOR_XP_SMALL; break;
                }
            }
            SpawnTextPopup(gem->pos, popupText, popupColor, popupScale);

            // Spawn UI particles flying to XP bar
            int numParticles = 1 + (int)gem->type;
            Color uiColor = COLOR_XP_BAR;
            for (int j = 0; j < numParticles; j++) {
                SpawnUIParticle(gem->pos, uiColor);
            }

            // Screen flash for large gems or combos
            if (gem->type == XP_LARGE) {
                g_game.screenFlash = 0.15f;
                g_game.screenFlashColor = COLOR_XP_LARGE;
            } else if (g_game.xpCombo > 3) {
                g_game.screenFlash = 0.08f;
                g_game.screenFlashColor = COLOR_XP_MEDIUM;
            }

            gem->active = false;

            // Enhanced particle burst
            int burstCount = 5 + (int)gem->type * 3;
            Color burstColor;
            switch (gem->type) {
                case XP_LARGE: burstColor = COLOR_XP_LARGE; break;
                case XP_MEDIUM: burstColor = COLOR_XP_MEDIUM; break;
                default: burstColor = COLOR_PARTICLE_XP; break;
            }
            SpawnParticleBurst(gem->pos, burstCount, burstColor, 80 + (int)gem->type * 20, 4 + (int)gem->type);

            // XP bar pulse
            g_game.xpBarPulse = 1.0f;

            if (player->level < MAX_LEVEL && player->xp >= player->xpToNextLevel) {
                player->xp -= player->xpToNextLevel;
                player->level++;
                player->upgradePoints++;

                // Check for VICTORY at level 20!
                if (player->level >= MAX_LEVEL) {
                    // YOU WIN! Reached max level!
                    g_game.state = GAME_STATE_VICTORY;
                    g_gameOverEntrance = 0;  // Reuse for victory animation
                    g_statCountUp = 0;

                    // Epic victory celebration
                    g_levelUpCelebration = 1.0f;
                    g_game.screenFlash = 1.0f;
                    g_game.screenFlashColor = (Color){255, 215, 0, 255};  // Gold flash
                    g_game.screenShake = 0.5f;

                    // Massive particle burst
                    for (int j = 0; j < 48; j++) {
                        float angle = (float)j / 48.0f * PI * 2.0f;
                        float speed = 300.0f + RandomFloat(0, 200);
                        Vector2 vel = {cosf(angle) * speed, sinf(angle) * speed};
                        Color pColor = (j % 3 == 0) ? (Color){255, 215, 0, 255} :
                                       (j % 3 == 1) ? COLOR_XP_BAR : (Color){255, 255, 255, 255};
                        SpawnParticle(player->pos, vel, pColor, RandomFloat(6, 12), 1.0f);
                    }
                } else {
                    player->xpToNextLevel = XP_THRESHOLDS[player->level - 1];
                    GenerateUpgradeChoices();
                    g_game.state = GAME_STATE_LEVEL_UP;

                    // Initialize multi-purchase session
                    g_game.sessionPointsRemaining = player->upgradePoints;
                    for (int k = 0; k < NUM_UPGRADE_CHOICES + 1; k++) {
                        g_game.upgradesPurchasedThisSession[k] = false;
                    }
                    g_game.levelUpMode = 0;  // Start in carousel mode
                    g_game.purchaseFlashTimer = 0;

                    // Level up celebration effects!
                    g_levelUpCelebration = 1.0f;
                    g_levelUpFreeze = LEVEL_UP_FREEZE_DURATION;
                    g_levelUpPos = player->pos;

                    // Radial particle burst
                    for (int j = 0; j < LEVEL_UP_BURST_PARTICLES; j++) {
                        float angle = (float)j / LEVEL_UP_BURST_PARTICLES * PI * 2.0f;
                        float speed = 200.0f + RandomFloat(0, 100);
                        Vector2 vel = {cosf(angle) * speed, sinf(angle) * speed};
                        // Alternating gold and cyan particles
                        Color pColor = (j % 2 == 0) ? (Color){255, 215, 0, 255} : COLOR_XP_BAR;
                        SpawnParticle(player->pos, vel, pColor, RandomFloat(4, 8), 0.6f);
                    }

                    // Screen flash for level up
                    g_game.screenFlash = 0.5f;
                    g_game.screenFlashColor = (Color){255, 255, 200, 100};
                    g_game.screenShake = 0.2f;
                }
            }
        }
    }
}

static void DrawXPGems(void) {
    for (int i = 0; i < MAX_XP_GEMS; i++) {
        XPGem *gem = &g_game.xpGems[i];
        if (!gem->active || !IsOnScreen(gem->pos, 40)) continue;

        Vector2 screen = WorldToScreen(gem->pos);
        float bob = sinf(gem->bobTimer) * 3.0f;
        float y = screen.y + bob;

        // Determine gem size and SDK color based on XP value
        float size = XP_GEM_SIZE;
        LlzGemColor gemColor;
        switch (gem->type) {
            case XP_LARGE:
                size *= 1.4f;
                gemColor = LLZ_GEM_TOPAZ;  // Golden for large
                break;
            case XP_MEDIUM:
                size *= 1.2f;
                gemColor = LLZ_GEM_SAPPHIRE;  // Blue for medium
                break;
            default:
                gemColor = LLZ_GEM_EMERALD;  // Green for small
                break;
        }

        // Pulsing glow effect
        float pulse = 0.6f + 0.4f * sinf(gem->glowTimer * 3.0f);

        // Get SDK color for glow effects
        Color baseColor = LlzGetGemColor(gemColor);

        // Outer glow (larger, softer)
        Color glowOuter = baseColor;
        glowOuter.a = (unsigned char)(40 * pulse);
        DrawCircleGradient((int)screen.x, (int)y, size * 3.0f * pulse, glowOuter, BLANK);

        // Inner glow
        Color glowInner = baseColor;
        glowInner.a = (unsigned char)(70 * pulse);
        DrawCircleGradient((int)screen.x, (int)y, size * 1.8f, glowInner, BLANK);

        // Magnetized state - brighter white pulse and size increase
        float magnetScale = 1.0f;
        if (gem->magnetized) {
            float magnetPulse = 0.5f + 0.5f * sinf(gem->glowTimer * 8.0f);
            magnetScale = 1.0f + 0.2f * magnetPulse;
            Color magnetGlow = WHITE;
            magnetGlow.a = (unsigned char)(60 * magnetPulse);
            DrawCircleGradient((int)screen.x, (int)y, size * 2.5f * magnetScale, magnetGlow, BLANK);
        }

        // Draw SDK gem shape (diamond shape with facets)
        LlzDrawGemShape(LLZ_SHAPE_DIAMOND, screen.x, y, size * magnetScale, gemColor);

        // Highlight sparkle at top
        float sparkle = fmaxf(0, sinf(gem->sparkleTimer * 5.0f));
        if (sparkle > 0.7f) {
            Color white = LlzGetGemColorLight(gemColor);
            white.a = (unsigned char)(200 * (sparkle - 0.7f) / 0.3f);
            DrawCircleV((Vector2){screen.x, y - size + 2}, 2.0f * sparkle, white);
        }
    }
}

// =============================================================================
// ENEMIES
// =============================================================================

static int CalculateEnemyHP(int baseHP) {
    return baseHP + (int)(g_game.gameTime * HP_SCALE_RATE);
}

static void SpawnEnemy(EnemyType type) {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        Enemy *e = &g_game.enemies[i];
        if (!e->active) {
            e->type = type;
            e->active = true;
            e->hitFlash = 0;
            // Initialize slow effect (no slow by default)
            e->slowTimer = 0;
            e->slowMultiplier = 1.0f;

            float spawnDist = 500.0f + RandomFloat(0, 200);
            float angle = RandomFloat(0, PI * 2);
            e->pos.x = Clampf(g_game.player.pos.x + cosf(angle) * spawnDist, WORLD_PADDING, WORLD_WIDTH - WORLD_PADDING);
            e->pos.y = Clampf(g_game.player.pos.y + sinf(angle) * spawnDist, WORLD_PADDING, WORLD_HEIGHT - WORLD_PADDING);

            float diff = g_game.spawner.difficultyMultiplier;
            switch (type) {
                case ENEMY_WALKER:
                    e->size = WALKER_SIZE; e->speed = WALKER_SPEED * (1.0f + diff * 0.2f);
                    e->hp = e->maxHp = CalculateEnemyHP(WALKER_BASE_HP);
                    e->damage = WALKER_DAMAGE; e->xpValue = WALKER_XP; break;
                case ENEMY_FAST:
                    e->size = FAST_SIZE; e->speed = FAST_SPEED * (1.0f + diff * 0.15f);
                    e->hp = e->maxHp = CalculateEnemyHP(FAST_BASE_HP);
                    e->damage = FAST_DAMAGE; e->xpValue = FAST_XP; break;
                case ENEMY_TANK:
                    e->size = TANK_SIZE; e->speed = TANK_SPEED * (1.0f + diff * 0.1f);
                    e->hp = e->maxHp = CalculateEnemyHP(TANK_BASE_HP) + (int)(g_game.gameTime * 0.1f);
                    e->damage = TANK_DAMAGE; e->xpValue = TANK_XP; break;
                case ENEMY_SWARM:
                    e->size = SWARM_SIZE; e->speed = SWARM_SPEED * (1.0f + diff * 0.25f);
                    e->hp = e->maxHp = CalculateEnemyHP(SWARM_BASE_HP);
                    e->damage = SWARM_DAMAGE; e->xpValue = SWARM_XP; break;
                case ENEMY_ELITE:
                    e->size = ELITE_SIZE; e->speed = ELITE_SPEED * (1.0f + diff * 0.15f);
                    e->hp = e->maxHp = CalculateEnemyHP(ELITE_BASE_HP) + (int)(g_game.gameTime * 0.05f);
                    e->damage = ELITE_DAMAGE; e->xpValue = ELITE_XP; break;
                case ENEMY_HORNET:
                    e->size = HORNET_SIZE; e->speed = HORNET_SPEED * (1.0f + diff * 0.1f);
                    e->hp = e->maxHp = CalculateEnemyHP(HORNET_BASE_HP);
                    e->damage = HORNET_DAMAGE; e->xpValue = HORNET_XP;
                    // Initialize laser state
                    e->laserCooldown = 0.5f;  // Short delay before first attack
                    e->laserChargeTimer = 0;
                    e->laserActiveTimer = 0;
                    e->laserAngle = 0;
                    e->laserCharging = false;
                    e->laserFiring = false;
                    break;
                case ENEMY_BRUTE:
                    e->size = BRUTE_SIZE; e->speed = BRUTE_SPEED * (1.0f + diff * 0.08f);
                    e->hp = e->maxHp = CalculateEnemyHP(BRUTE_BASE_HP) + (int)(g_game.gameTime * 0.15f);
                    e->damage = BRUTE_DAMAGE; e->xpValue = BRUTE_XP; break;
                case ENEMY_BOSS:
                    e->size = BOSS_SIZE; e->speed = BOSS_SPEED * (1.0f + diff * 0.05f);
                    e->hp = e->maxHp = CalculateEnemyHP(BOSS_BASE_HP) + (int)(g_game.gameTime * 0.2f);
                    e->damage = BOSS_DAMAGE; e->xpValue = BOSS_XP; break;
                // Bullet hell enemies
                case ENEMY_SPINNER:
                    e->size = SPINNER_SIZE; e->speed = SPINNER_SPEED * (1.0f + diff * 0.1f);
                    e->hp = e->maxHp = CalculateEnemyHP(SPINNER_BASE_HP);
                    e->damage = SPINNER_DAMAGE; e->xpValue = SPINNER_XP;
                    e->spinAngle = 0;
                    e->attackTimer = SPINNER_COOLDOWN * 0.5f;  // Start halfway through cooldown
                    e->bulletsFired = 0;
                    e->isVulnerable = false;
                    e->vulnerableTimer = 0;
                    break;
                case ENEMY_MIRROR:
                    e->size = MIRROR_SIZE; e->speed = MIRROR_SPEED * (1.0f + diff * 0.15f);
                    e->hp = e->maxHp = CalculateEnemyHP(MIRROR_BASE_HP);
                    e->damage = MIRROR_DAMAGE; e->xpValue = MIRROR_XP;
                    e->isDecoy = false;
                    e->realEnemyIdx = -1;
                    e->revealTimer = 0;
                    e->splitTimer = MIRROR_SPLIT_COOLDOWN * 0.3f;  // Delay before first split
                    break;
                case ENEMY_SHIELDER:
                    e->size = SHIELDER_SIZE; e->speed = SHIELDER_SPEED * (1.0f + diff * 0.1f);
                    e->hp = e->maxHp = CalculateEnemyHP(SHIELDER_BASE_HP);
                    e->damage = SHIELDER_DAMAGE; e->xpValue = SHIELDER_XP;
                    e->shieldAngle = atan2f(g_game.player.pos.y - e->pos.y, g_game.player.pos.x - e->pos.x);
                    e->isCharging = false;
                    e->chargeTimer = SHIELDER_CHARGE_COOLDOWN;
                    e->chargeDir = (Vector2){0, 0};
                    break;
                case ENEMY_BOMBER:
                    e->size = BOMBER_SIZE; e->speed = BOMBER_SPEED * (1.0f + diff * 0.12f);
                    e->hp = e->maxHp = CalculateEnemyHP(BOMBER_BASE_HP);
                    e->damage = BOMBER_DAMAGE; e->xpValue = BOMBER_XP;
                    e->dropTimer = BOMBER_DROP_COOLDOWN * 0.5f;
                    e->stunnedTimer = 0;
                    break;
                case ENEMY_PHASER:
                    e->size = PHASER_SIZE; e->speed = PHASER_SPEED * (1.0f + diff * 0.15f);
                    e->hp = e->maxHp = CalculateEnemyHP(PHASER_BASE_HP);
                    e->damage = PHASER_DAMAGE; e->xpValue = PHASER_XP;
                    e->phaseTimer = PHASER_VISIBLE_DURATION;  // Start visible
                    e->isPhased = false;
                    e->visibility = 1.0f;
                    break;
                default: break;
            }

            // Initialize champion state (defaults to non-champion)
            e->isChampion = false;
            e->affix = AFFIX_NONE;
            e->championGlow = RandomFloat(0, PI * 2);  // Random phase for glow animation
            e->baseSpeed = e->speed;  // Store original speed before any modifications

            // Champion spawn logic: after wave 3, 5% chance to become champion
            // Don't make bosses into champions (they're already special)
            // Don't make decoys into champions
            if (g_game.spawner.wave >= CHAMPION_SPAWN_WAVE &&
                type != ENEMY_BOSS &&
                !e->isDecoy &&
                GetRandomValue(0, 99) < CHAMPION_SPAWN_CHANCE) {

                e->isChampion = true;
                e->affix = (EnemyAffix)(GetRandomValue(1, AFFIX_COUNT - 1));  // Random affix (not AFFIX_NONE)

                // Apply champion stat modifiers
                e->hp = (int)(e->hp * CHAMPION_HP_MULTIPLIER);
                e->maxHp = (int)(e->maxHp * CHAMPION_HP_MULTIPLIER);
                e->xpValue = (int)(e->xpValue * CHAMPION_XP_MULTIPLIER);

                // Apply affix-specific modifiers
                if (e->affix == AFFIX_SWIFT) {
                    e->speed *= (1.0f + AFFIX_SWIFT_SPEED_BONUS);
                    e->baseSpeed = e->speed;
                }
            }

            return;
        }
    }
}

// Spawn swarm (multiple small enemies at once)
static void SpawnSwarm(void) {
    float baseAngle = RandomFloat(0, PI * 2);
    float spawnDist = 500.0f + RandomFloat(0, 150);

    for (int i = 0; i < SWARM_SPAWN_COUNT; i++) {
        float angleOffset = (float)i / SWARM_SPAWN_COUNT * PI * 0.5f - PI * 0.25f;
        float angle = baseAngle + angleOffset;

        for (int j = 0; j < MAX_ENEMIES; j++) {
            Enemy *e = &g_game.enemies[j];
            if (!e->active) {
                e->type = ENEMY_SWARM;
                e->active = true;
                e->hitFlash = 0;
                e->pos.x = Clampf(g_game.player.pos.x + cosf(angle) * spawnDist, WORLD_PADDING, WORLD_WIDTH - WORLD_PADDING);
                e->pos.y = Clampf(g_game.player.pos.y + sinf(angle) * spawnDist, WORLD_PADDING, WORLD_HEIGHT - WORLD_PADDING);
                e->size = SWARM_SIZE;
                e->speed = SWARM_SPEED * (1.0f + g_game.spawner.difficultyMultiplier * 0.25f);
                e->hp = e->maxHp = CalculateEnemyHP(SWARM_BASE_HP);
                e->damage = SWARM_DAMAGE;
                e->xpValue = SWARM_XP;
                break;
            }
        }
    }
}

// =============================================================================
// ENEMY BULLETS AND MINES (Bullet Hell Systems)
// =============================================================================

static void SpawnEnemyBullet(Vector2 pos, float angle, int damage, float speed) {
    for (int i = 0; i < MAX_ENEMY_BULLETS; i++) {
        EnemyBullet *b = &g_game.enemyBullets[i];
        if (!b->active) {
            b->pos = pos;
            b->vel.x = cosf(angle) * speed;
            b->vel.y = sinf(angle) * speed;
            b->damage = damage;
            b->size = ENEMY_BULLET_SIZE;
            b->active = true;
            b->color = COLOR_ENEMY_BULLET;
            return;
        }
    }
}

static void SpawnMine(Vector2 pos, int damage, float radius) {
    for (int i = 0; i < MAX_MINES; i++) {
        Mine *m = &g_game.mines[i];
        if (!m->active) {
            m->pos = pos;
            m->timer = BOMBER_MINE_DELAY;
            m->damage = damage;
            m->radius = radius;
            m->active = true;
            m->exploding = false;
            m->explodeTimer = 0;
            return;
        }
    }
}

static void UpdateEnemyBullets(float dt) {
    Player *player = &g_game.player;
    for (int i = 0; i < MAX_ENEMY_BULLETS; i++) {
        EnemyBullet *b = &g_game.enemyBullets[i];
        if (!b->active) continue;

        b->pos.x += b->vel.x * dt;
        b->pos.y += b->vel.y * dt;

        // Check collision with player
        float dist = Distance(b->pos, player->pos);
        float collisionDist = b->size / 2 + PLAYER_SIZE / 2;
        if (dist < collisionDist && player->invincibilityTimer <= 0 && !HasShield()) {
            DamagePlayer(b->damage, b->pos);
            b->active = false;
            SpawnParticleBurst(b->pos, 4, COLOR_ENEMY_BULLET, 40, 2);
            // Reset graze combo on hit
            g_game.grazeCombo = 0;
            continue;
        }

        // Graze detection - near miss bonus
        float grazeDist = collisionDist * GRAZE_DISTANCE_MULTIPLIER;
        if (dist < grazeDist && dist >= collisionDist && player->invincibilityTimer <= 0) {
            // Check if bullet is moving away (already passed player)
            Vector2 toPlayer = {player->pos.x - b->pos.x, player->pos.y - b->pos.y};
            float dotProduct = toPlayer.x * b->vel.x + toPlayer.y * b->vel.y;

            // Only graze if bullet is moving away (negative dot = moving away)
            if (dotProduct < 0) {
                // Award graze bonus
                g_game.grazeCount++;
                g_game.grazeCombo++;
                g_game.grazeComboTimer = 1.5f;  // Reset combo timer
                g_game.grazeFlash = 0.3f;

                // XP bonus scales with combo
                int xpBonus = GRAZE_XP_BONUS * (1 + g_game.grazeCombo / 5);
                player->xp += xpBonus;

                // Visual feedback
                SpawnTextPopup(b->pos, "GRAZE!", (Color){255, 200, 100, 255}, 0.5f);
                SpawnParticleBurst(b->pos, 3, (Color){255, 220, 100, 200}, 30, 2);

                // Deactivate bullet after graze to prevent re-triggering
                b->active = false;
            }
        }

        // Remove if too far from player
        if (dist > 800.0f) {
            b->active = false;
        }
    }
}

static void UpdateMines(float dt) {
    Player *player = &g_game.player;
    for (int i = 0; i < MAX_MINES; i++) {
        Mine *m = &g_game.mines[i];
        if (!m->active) continue;

        if (m->exploding) {
            m->explodeTimer -= dt;
            if (m->explodeTimer <= 0) {
                m->active = false;
            }
            continue;
        }

        m->timer -= dt;
        if (m->timer <= 0) {
            // Explode!
            m->exploding = true;
            m->explodeTimer = 0.3f;  // Explosion animation duration

            // Damage player if in range
            float dist = Distance(m->pos, player->pos);
            if (dist < m->radius && player->invincibilityTimer <= 0 && !HasShield()) {
                DamagePlayer(m->damage, m->pos);
            }

            // Explosion effects
            SpawnParticleBurst(m->pos, 12, COLOR_MINE, 100, 5);
            g_game.screenShake = fmaxf(g_game.screenShake, 0.2f);
        }
    }
}

// Spawn a mirror decoy at position
static void SpawnMirrorDecoy(Vector2 pos, int realIdx) {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        Enemy *e = &g_game.enemies[i];
        if (!e->active) {
            e->type = ENEMY_MIRROR;
            e->active = true;
            e->hitFlash = 0;
            e->slowTimer = 0;
            e->slowMultiplier = 1.0f;
            e->pos = pos;
            e->size = MIRROR_SIZE;
            e->speed = MIRROR_SPEED;
            e->hp = e->maxHp = 1;  // Decoys die in one hit
            e->damage = MIRROR_DAMAGE;
            e->xpValue = 0;  // No XP from decoys
            e->isDecoy = true;
            e->realEnemyIdx = realIdx;
            e->revealTimer = 0;
            e->splitTimer = MIRROR_DECOY_DURATION;  // Use splitTimer as lifetime
            return;
        }
    }
}

static void DamageEnemy(Enemy *e, int damage) {
    // Special vulnerability checks for bullet hell enemies
    if (e->type == ENEMY_SPINNER && !e->isVulnerable) {
        // Spinner is invulnerable when eye is closed
        SpawnParticleBurst(e->pos, 2, (Color){100, 100, 100, 255}, 30, 2);
        SpawnTextPopup(e->pos, "BLOCKED", (Color){150, 150, 150, 255}, 0.8f);
        return;
    }
    if (e->type == ENEMY_PHASER && e->isPhased) {
        // Phaser is invulnerable when phased out
        SpawnParticleBurst(e->pos, 2, (Color){150, 100, 200, 100}, 30, 2);
        return;
    }
    if (e->type == ENEMY_SHIELDER) {
        // Check if attack is blocked by shield
        float attackAngle = atan2f(g_game.player.pos.y - e->pos.y, g_game.player.pos.x - e->pos.x);
        float angleDiff = attackAngle - e->shieldAngle;
        // Normalize angle difference
        while (angleDiff > PI) angleDiff -= PI * 2;
        while (angleDiff < -PI) angleDiff += PI * 2;
        // Shield blocks attacks from the front (within shield arc)
        if (fabsf(angleDiff) < (SHIELDER_SHIELD_ARC / 2.0f) * DEG2RAD) {
            SpawnParticleBurst(e->pos, 3, COLOR_SHIELD, 40, 2);
            SpawnTextPopup(e->pos, "BLOCKED", COLOR_SHIELD, 0.8f);
            return;
        }
    }

    int finalDamage = (int)(damage * GetDamageMultiplier());
    bool wasCrit = g_lastHitWasCrit;  // Capture crit state before next GetDamageMultiplier call

    // Champion ARMORED affix: 50% damage reduction
    if (e->isChampion && e->affix == AFFIX_ARMORED) {
        finalDamage = (int)(finalDamage * (1.0f - AFFIX_ARMORED_REDUCTION));
        if (finalDamage < 1) finalDamage = 1;  // Minimum 1 damage
    }

    e->hp -= finalDamage;
    e->hitFlash = 0.1f;
    SpawnParticleBurst(e->pos, 3, COLOR_PARTICLE_HIT, 60, 3);

    // Spawn damage number popup with crit coloring
    char dmgText[16];
    if (wasCrit) {
        snprintf(dmgText, sizeof(dmgText), "%d!", finalDamage);
        // Gold/yellow color for crits, larger scale
        SpawnTextPopup(e->pos, dmgText, (Color){255, 215, 0, 255}, 1.4f);
        // Extra particles for crit hits
        SpawnParticleBurst(e->pos, 5, (Color){255, 215, 0, 255}, 80, 4);
    } else {
        snprintf(dmgText, sizeof(dmgText), "%d", finalDamage);
        SpawnTextPopup(e->pos, dmgText, WHITE, 1.0f);
    }

    // Lifesteal: heal player for % of damage dealt (with exponential dropoff)
    // Raw lifesteal is capped at 50%, but effective lifesteal has diminishing returns
    // Formula: effective = max_rate * (1 - e^(-raw / scale))
    // At 10% raw -> ~6.3% effective, at 25% raw -> ~11.8% effective, at 50% raw -> ~15.4% effective
    if (g_game.player.lifesteal > 0) {
        float rawLifesteal = g_game.player.lifesteal;
        float maxEffective = 18.0f;  // Maximum effective lifesteal percent
        float scaleFactor = 20.0f;   // Controls curve steepness
        float effectiveLifesteal = maxEffective * (1.0f - expf(-rawLifesteal / scaleFactor));

        int healAmount = (int)(finalDamage * effectiveLifesteal / 100.0f);
        if (healAmount > 0) {
            g_game.player.hp += healAmount;
            if (g_game.player.hp > g_game.player.maxHp) {
                g_game.player.hp = g_game.player.maxHp;
            }
        }
    }

    if (e->hp <= 0) {
        // Get enemy color for death animation before deactivating
        Color deathColor;
        switch (e->type) {
            case ENEMY_WALKER: deathColor = COLOR_WALKER; break;
            case ENEMY_FAST: deathColor = COLOR_FAST; break;
            case ENEMY_TANK: deathColor = COLOR_TANK; break;
            default: deathColor = COLOR_WALKER; break;
        }

        // Spawn dying enemy animation
        SpawnDyingEnemy(e->pos, e->type, e->size, deathColor);

        // Champion SPLITTER affix: spawn smaller copies on death
        if (e->isChampion && e->affix == AFFIX_SPLITTER) {
            for (int s = 0; s < AFFIX_SPLITTER_COUNT; s++) {
                // Find an inactive enemy slot
                for (int j = 0; j < MAX_ENEMIES; j++) {
                    Enemy *spawn = &g_game.enemies[j];
                    if (!spawn->active) {
                        // Copy parent enemy properties
                        spawn->type = e->type;
                        spawn->active = true;
                        spawn->hitFlash = 0;
                        spawn->slowTimer = 0;
                        spawn->slowMultiplier = 1.0f;

                        // Position slightly offset from parent
                        float spawnAngle = (float)s / AFFIX_SPLITTER_COUNT * PI * 2 + RandomFloat(-0.5f, 0.5f);
                        float spawnDist = e->size * 0.8f;
                        spawn->pos.x = Clampf(e->pos.x + cosf(spawnAngle) * spawnDist, WORLD_PADDING, WORLD_WIDTH - WORLD_PADDING);
                        spawn->pos.y = Clampf(e->pos.y + sinf(spawnAngle) * spawnDist, WORLD_PADDING, WORLD_HEIGHT - WORLD_PADDING);

                        // Smaller stats for split copies
                        spawn->size = e->size * AFFIX_SPLITTER_SIZE_RATIO;
                        spawn->speed = e->baseSpeed * 1.1f;  // Slightly faster
                        spawn->baseSpeed = spawn->speed;
                        spawn->hp = (int)(e->maxHp * AFFIX_SPLITTER_HP_RATIO);
                        spawn->maxHp = spawn->hp;
                        spawn->damage = e->damage / 2;
                        spawn->xpValue = e->xpValue / 3;  // Less XP since champion already gave bonus

                        // Split copies are NOT champions (no infinite splitting!)
                        spawn->isChampion = false;
                        spawn->affix = AFFIX_NONE;
                        spawn->championGlow = 0;

                        // Copy special state if needed
                        spawn->isDecoy = false;
                        spawn->laserCooldown = 0;
                        spawn->laserCharging = false;
                        spawn->laserFiring = false;

                        // Visual feedback for split
                        SpawnParticleBurst(spawn->pos, 4, (Color){255, 200, 100, 255}, 50, 3);
                        break;
                    }
                }
            }
            // Extra particles for splitter death
            SpawnParticleBurst(e->pos, 6, (Color){255, 200, 100, 255}, 80, 4);
        }

        e->active = false;
        g_game.killCount++;
        RegisterKill();  // Track kill streak
        CheckMilestones();  // Check for milestone rewards

        SpawnParticleBurst(e->pos, 8, COLOR_PARTICLE_DIE, 100, 5);
        // Apply danger zone XP bonus
        float dangerMult = GetDangerZoneXPMultiplier(e->pos);
        int bonusXP = (int)(e->xpValue * dangerMult);
        SpawnXPGem(e->pos, bonusXP);
        g_game.screenShake = 0.1f;

        // Hitstop: brief freeze on kill for impactful feel
        g_hitstopTimer = HITSTOP_DURATION;

        if (GetRandomValue(0, 100) < POTION_DROP_CHANCE) {
            SpawnPotion(e->pos);
        }
    }
}

// Apply damage to player with armor and dodge
static void DamagePlayer(int damage, Vector2 knockbackFrom) {
    Player *player = &g_game.player;

    // Check dodge
    if (player->dodgeChance > 0 && GetRandomValue(0, 100) < (int)player->dodgeChance) {
        SpawnParticleBurst(player->pos, 4, COLOR_TEXT, 50, 3);
        return;  // Dodged!
    }

    // Apply armor damage reduction
    int finalDamage = damage;
    if (player->armor > 0) {
        finalDamage = (int)(damage * (1.0f - player->armor / 100.0f));
        if (finalDamage < 1) finalDamage = 1;
    }

    player->hp -= finalDamage;
    player->invincibilityTimer = PLAYER_INVINCIBILITY_TIME;
    player->hurtFlash = 0.2f;
    g_game.screenShake = 0.15f;

    // Red damage vignette - intensity based on damage taken
    float vignetteIntensity = Clampf((float)finalDamage / 20.0f, 0.3f, 1.0f);
    g_damageVignette = fmaxf(g_damageVignette, vignetteIntensity);

    // Knockback
    Vector2 knock = Normalize((Vector2){player->pos.x - knockbackFrom.x, player->pos.y - knockbackFrom.y});
    player->pos.x += knock.x * 30;
    player->pos.y += knock.y * 30;

    if (player->hp <= 0) {
        g_game.state = GAME_STATE_GAME_OVER;
        // Reset game over entrance animation
        g_gameOverEntrance = 0;
        g_statCountUp = 0;
        g_displayedKills = 0;
        g_displayedTime = 0;
    }
}

// =============================================================================
// Enemy AI Function Pointer System
// =============================================================================
// Each enemy type can have a unique AI behavior function. This avoids large
// switch statements and allows easy addition of new enemy types.

typedef void (*EnemyAIFunc)(Enemy *e, Player *player, float dt, Vector2 dir, float dist, float effectiveSpeed);

// Default chase behavior for basic enemies
static void AI_Chase(Enemy *e, Player *player, float dt, Vector2 dir, float dist, float effectiveSpeed) {
    (void)player; (void)dist;  // Unused
    e->pos.x += dir.x * effectiveSpeed * dt;
    e->pos.y += dir.y * effectiveSpeed * dt;
}

// Hornet: stops at range and fires lasers
static void AI_Hornet(Enemy *e, Player *player, float dt, Vector2 dir, float dist, float effectiveSpeed) {
    if (dist > HORNET_ATTACK_RANGE) {
        e->pos.x += dir.x * effectiveSpeed * dt;
        e->pos.y += dir.y * effectiveSpeed * dt;
        e->laserCharging = false;
        e->laserFiring = false;
    } else {
        // Manage laser state machine
        if (e->laserFiring) {
            e->laserActiveTimer -= dt;
            if (e->laserActiveTimer <= 0) {
                e->laserFiring = false;
                e->laserCooldown = HORNET_LASER_COOLDOWN;
            }
        } else if (e->laserCharging) {
            e->laserAngle = atan2f(dir.y, dir.x);
            e->laserChargeTimer -= dt;
            if (e->laserChargeTimer <= 0) {
                e->laserCharging = false;
                e->laserFiring = true;
                e->laserActiveTimer = HORNET_LASER_DURATION;
            }
        } else if (e->laserCooldown > 0) {
            e->laserCooldown -= dt;
        } else {
            e->laserCharging = true;
            e->laserChargeTimer = HORNET_LASER_CHARGE_TIME;
            e->laserAngle = atan2f(dir.y, dir.x);
        }
    }
    // Laser damage check
    if (e->laserFiring && player->invincibilityTimer <= 0 && !HasShield()) {
        float laserLength = 500.0f;
        Vector2 laserEnd = {e->pos.x + cosf(e->laserAngle) * laserLength, e->pos.y + sinf(e->laserAngle) * laserLength};
        float dx = laserEnd.x - e->pos.x, dy = laserEnd.y - e->pos.y;
        float lineLenSq = dx * dx + dy * dy;
        float t = fmaxf(0, fminf(1, ((player->pos.x - e->pos.x) * dx + (player->pos.y - e->pos.y) * dy) / lineLenSq));
        Vector2 closest = {e->pos.x + t * dx, e->pos.y + t * dy};
        if (Distance(player->pos, closest) < HORNET_LASER_WIDTH / 2 + PLAYER_SIZE / 2) {
            DamagePlayer(HORNET_LASER_DAMAGE, e->pos);
        }
    }
}

// Spinner: stops at range, spins, fires spiral bullet patterns
static void AI_Spinner(Enemy *e, Player *player, float dt, Vector2 dir, float dist, float effectiveSpeed) {
    (void)player;  // Unused
    e->spinAngle += 2.0f * dt;  // Always rotating
    if (dist > SPINNER_ATTACK_RANGE) {
        e->pos.x += dir.x * effectiveSpeed * dt;
        e->pos.y += dir.y * effectiveSpeed * dt;
    } else {
        // Attack cycle
        if (e->isVulnerable) {
            // Eye is open - vulnerable period
            e->vulnerableTimer -= dt;
            if (e->vulnerableTimer <= 0) {
                e->isVulnerable = false;
                e->attackTimer = 0;
                e->bulletsFired = 0;
            }
        } else if (e->bulletsFired < SPINNER_BARRAGE_COUNT) {
            // Firing barrage
            e->attackTimer -= dt;
            if (e->attackTimer <= 0) {
                // Fire bullet in spiral pattern
                float bulletAngle = e->spinAngle + (e->bulletsFired * PI * 2.0f / SPINNER_BULLETS_PER_WAVE);
                SpawnEnemyBullet(e->pos, bulletAngle, SPINNER_BULLET_DAMAGE, SPINNER_BULLET_SPEED);
                e->bulletsFired++;
                e->attackTimer = SPINNER_FIRE_RATE;
            }
        } else {
            // Barrage complete - become vulnerable
            e->isVulnerable = true;
            e->vulnerableTimer = SPINNER_VULNERABLE_TIME;
        }
    }
}

// Mirror: creates decoys, real one has reveal timer
static void AI_Mirror(Enemy *e, Player *player, float dt, Vector2 dir, float dist, float effectiveSpeed) {
    (void)player; (void)dist;  // Unused
    // Normal movement
    e->pos.x += dir.x * effectiveSpeed * dt;
    e->pos.y += dir.y * effectiveSpeed * dt;

    if (e->isDecoy) {
        // Decoy lifetime countdown
        e->splitTimer -= dt;
        if (e->splitTimer <= 0) {
            e->active = false;  // Decoy fades
            SpawnParticleBurst(e->pos, 4, COLOR_MIRROR, 50, 3);
        }
    } else {
        // Real mirror - manage split timer
        e->revealTimer -= dt;
        e->splitTimer -= dt;
        if (e->splitTimer <= 0) {
            // Create decoys
            for (int d = 0; d < MIRROR_DECOY_COUNT; d++) {
                float offsetAngle = RandomFloat(0, PI * 2);
                float offsetDist = 50.0f + RandomFloat(0, 50);
                Vector2 decoyPos = {
                    e->pos.x + cosf(offsetAngle) * offsetDist,
                    e->pos.y + sinf(offsetAngle) * offsetDist
                };
                // Find this enemy's index for the decoy parent reference
                int parentIdx = (int)(e - g_game.enemies);
                SpawnMirrorDecoy(decoyPos, parentIdx);
            }
            e->splitTimer = MIRROR_SPLIT_COOLDOWN;
            // Brief reveal after split
            e->revealTimer = MIRROR_REVEAL_TIME;
        }
    }
}

// Shielder: rotating shield, charge attack
static void AI_Shielder(Enemy *e, Player *player, float dt, Vector2 dir, float dist, float effectiveSpeed) {
    (void)player;  // Unused
    // Rotate shield to face player
    float targetAngle = atan2f(dir.y, dir.x);
    float angleDiff = targetAngle - e->shieldAngle;
    // Normalize angle difference
    while (angleDiff > PI) angleDiff -= PI * 2;
    while (angleDiff < -PI) angleDiff += PI * 2;
    e->shieldAngle += angleDiff * SHIELDER_ROTATE_SPEED * dt;

    if (e->isCharging) {
        // Charging toward player
        e->pos.x += e->chargeDir.x * SHIELDER_CHARGE_SPEED * dt;
        e->pos.y += e->chargeDir.y * SHIELDER_CHARGE_SPEED * dt;
        e->chargeTimer -= dt;
        if (e->chargeTimer <= 0) {
            e->isCharging = false;
            e->chargeTimer = SHIELDER_CHARGE_COOLDOWN;
        }
    } else {
        // Normal movement
        e->pos.x += dir.x * effectiveSpeed * dt;
        e->pos.y += dir.y * effectiveSpeed * dt;
        e->chargeTimer -= dt;
        if (e->chargeTimer <= 0 && dist < 200.0f) {
            // Start charge attack
            e->isCharging = true;
            e->chargeDir = dir;
            e->chargeTimer = SHIELDER_CHARGE_DURATION;
        }
    }
}

// Bomber: drops mines, stunned after dropping
static void AI_Bomber(Enemy *e, Player *player, float dt, Vector2 dir, float dist, float effectiveSpeed) {
    (void)player;  // Unused
    if (e->stunnedTimer > 0) {
        e->stunnedTimer -= dt;
    } else {
        e->pos.x += dir.x * effectiveSpeed * dt;
        e->pos.y += dir.y * effectiveSpeed * dt;
        e->dropTimer -= dt;
        if (e->dropTimer <= 0 && dist < 300.0f) {
            // Drop mines in pattern
            for (int m = 0; m < BOMBER_MINES_PER_DROP; m++) {
                float mineAngle = (float)m / BOMBER_MINES_PER_DROP * PI * 2;
                Vector2 minePos = {
                    e->pos.x + cosf(mineAngle) * 30.0f,
                    e->pos.y + sinf(mineAngle) * 30.0f
                };
                SpawnMine(minePos, BOMBER_MINE_DAMAGE, BOMBER_MINE_RADIUS);
            }
            e->dropTimer = BOMBER_DROP_COOLDOWN;
            e->stunnedTimer = BOMBER_VULNERABLE_AFTER_DROP;
            SpawnParticleBurst(e->pos, 6, COLOR_MINE, 40, 3);
        }
    }
}

// Phaser: phases in/out, teleports near player
static void AI_Phaser(Enemy *e, Player *player, float dt, Vector2 dir, float dist, float effectiveSpeed) {
    (void)dist;  // Unused
    e->phaseTimer -= dt;
    if (e->isPhased) {
        // Phased out - invisible and invulnerable
        e->visibility = fmaxf(0, e->visibility - dt * 3.0f);
        if (e->phaseTimer <= 0) {
            // Phase in - teleport near player and fire burst
            float teleAngle = RandomFloat(0, PI * 2);
            e->pos.x = player->pos.x + cosf(teleAngle) * PHASER_TELEPORT_RANGE;
            e->pos.y = player->pos.y + sinf(teleAngle) * PHASER_TELEPORT_RANGE;
            e->pos.x = Clampf(e->pos.x, WORLD_PADDING, WORLD_WIDTH - WORLD_PADDING);
            e->pos.y = Clampf(e->pos.y, WORLD_PADDING, WORLD_HEIGHT - WORLD_PADDING);

            // Fire bullet burst
            for (int b = 0; b < PHASER_BULLETS_ON_APPEAR; b++) {
                float bulletAngle = (float)b / PHASER_BULLETS_ON_APPEAR * PI * 2;
                SpawnEnemyBullet(e->pos, bulletAngle, PHASER_BULLET_DAMAGE, PHASER_BULLET_SPEED);
            }
            SpawnParticleBurst(e->pos, 8, COLOR_PHASER, 80, 4);

            e->isPhased = false;
            e->phaseTimer = PHASER_VISIBLE_DURATION;
        }
    } else {
        // Visible - normal movement
        e->visibility = fminf(1, e->visibility + dt * 3.0f);
        e->pos.x += dir.x * effectiveSpeed * dt;
        e->pos.y += dir.y * effectiveSpeed * dt;
        if (e->phaseTimer <= 0) {
            // Phase out
            e->isPhased = true;
            e->phaseTimer = PHASER_PHASE_DURATION;
            SpawnParticleBurst(e->pos, 6, COLOR_PHASER, 60, 3);
        }
    }
}

// AI function lookup table indexed by EnemyType
static const EnemyAIFunc ENEMY_AI_FUNCS[ENEMY_TYPE_COUNT] = {
    [ENEMY_WALKER]   = AI_Chase,
    [ENEMY_FAST]     = AI_Chase,
    [ENEMY_TANK]     = AI_Chase,
    [ENEMY_SWARM]    = AI_Chase,
    [ENEMY_ELITE]    = AI_Chase,
    [ENEMY_HORNET]   = AI_Hornet,
    [ENEMY_BRUTE]    = AI_Chase,
    [ENEMY_SPINNER]  = AI_Spinner,
    [ENEMY_MIRROR]   = AI_Mirror,
    [ENEMY_SHIELDER] = AI_Shielder,
    [ENEMY_BOSS]     = AI_Chase,
    [ENEMY_BOMBER]   = AI_Bomber,
    [ENEMY_PHASER]   = AI_Phaser,
};

// =============================================================================
// End Enemy AI System
// =============================================================================

static void UpdateEnemies(float dt) {
    Player *player = &g_game.player;

    // Reset danger glow accumulator
    float dangerLeft = 0, dangerRight = 0, dangerTop = 0, dangerBottom = 0;

    for (int i = 0; i < MAX_ENEMIES; i++) {
        Enemy *e = &g_game.enemies[i];
        if (!e->active) continue;

        e->hitFlash -= dt;
        if (e->hitFlash < 0) e->hitFlash = 0;

        // Process slow effect timer
        if (e->slowTimer > 0) {
            e->slowTimer -= dt;
            if (e->slowTimer <= 0) {
                e->slowMultiplier = 1.0f;  // Reset to normal speed
            }
        }

        // Champion system: update glow animation and handle VAMPIRIC regen
        if (e->isChampion) {
            e->championGlow += dt * 3.0f;  // Glow animation speed
            if (e->championGlow > PI * 2) e->championGlow -= PI * 2;

            // VAMPIRIC affix: regenerate HP over time
            if (e->affix == AFFIX_VAMPIRIC && e->hp < e->maxHp) {
                e->hp += (int)(AFFIX_VAMPIRIC_REGEN * dt);
                if (e->hp > e->maxHp) e->hp = e->maxHp;
            }
        }

        // Calculate effective speed (apply slow multiplier)
        float effectiveSpeed = e->speed * (e->slowMultiplier > 0 ? e->slowMultiplier : 1.0f);

        Vector2 dir = Normalize((Vector2){player->pos.x - e->pos.x, player->pos.y - e->pos.y});
        float dist = Distance(e->pos, player->pos);

        // Cache angle to player for drawing (avoid recalculating atan2f in draw)
        e->cachedAngle = atan2f(dir.y, dir.x);
        e->cachedAngleTime = g_game.gameTime;

        // Dispatch to enemy-specific AI via function pointer
        if (e->type >= 0 && e->type < ENEMY_TYPE_COUNT && ENEMY_AI_FUNCS[e->type]) {
            ENEMY_AI_FUNCS[e->type](e, player, dt, dir, dist, effectiveSpeed);
        }
        if (dist < (e->size / 2 + PLAYER_SIZE / 2) && player->invincibilityTimer <= 0 && !HasShield()) {
            DamagePlayer(e->damage, e->pos);

            // Thorns: reflect damage back to enemy
            if (player->thorns > 0) {
                int thornsDamage = (int)(e->damage * player->thorns / 100.0f);
                if (thornsDamage > 0) {
                    e->hp -= thornsDamage;
                    SpawnParticleBurst(e->pos, 3, COLOR_POTION_DAMAGE, 40, 2);
                    if (e->hp <= 0) {
                        e->active = false;
                        g_game.killCount++;
                        CheckMilestones();  // Check for milestone rewards
                        SpawnParticleBurst(e->pos, 6, COLOR_PARTICLE_DIE, 80, 4);
                        // Apply danger zone XP bonus
                        float dzMult = GetDangerZoneXPMultiplier(e->pos);
                        int dzBonusXP = (int)(e->xpValue * dzMult);
                        SpawnXPGem(e->pos, dzBonusXP);
                    }
                }
            }
        }

        if (dist > 1000.0f) e->active = false;

        // Calculate danger glow for off-screen but nearby enemies
        if (e->active && dist < DANGER_GLOW_RANGE) {
            Vector2 screen = WorldToScreen(e->pos);
            float intensity = 1.0f - (dist / DANGER_GLOW_RANGE);
            intensity *= intensity;  // Quadratic falloff for more urgency when close

            // Check which screen edge the enemy is near/beyond
            if (screen.x < 0) dangerLeft = fmaxf(dangerLeft, intensity);
            else if (screen.x > g_screenWidth) dangerRight = fmaxf(dangerRight, intensity);
            if (screen.y < 0) dangerTop = fmaxf(dangerTop, intensity);
            else if (screen.y > g_screenHeight) dangerBottom = fmaxf(dangerBottom, intensity);
        }
    }

    // Update danger glow with smooth lerp
    g_dangerGlow[0] = fmaxf(g_dangerGlow[0], dangerLeft);   // Left
    g_dangerGlow[1] = fmaxf(g_dangerGlow[1], dangerRight);  // Right
    g_dangerGlow[2] = fmaxf(g_dangerGlow[2], dangerTop);    // Top
    g_dangerGlow[3] = fmaxf(g_dangerGlow[3], dangerBottom); // Bottom
}

static void DrawEnemy(Enemy *e) {
    if (!IsOnScreen(e->pos, e->size)) return;
    Vector2 screen = WorldToScreen(e->pos);

    Color color = GetEnemyColor(e->type);
    if (e->hitFlash > 0) color = WHITE;

    // Add slow effect visual tint
    if (e->slowTimer > 0 && e->slowMultiplier < 1.0f) {
        // Purple tint for slowed enemies
        color.r = (unsigned char)(color.r * 0.7f);
        color.g = (unsigned char)(color.g * 0.7f + 50);
        color.b = (unsigned char)fminf(255, color.b * 0.8f + 80);
    }

    float hs = e->size / 2;

    // LOD: Use simplified rendering for enemies near screen edges
    float screenCenterDist = sqrtf((screen.x - g_screenWidth / 2) * (screen.x - g_screenWidth / 2) +
                                   (screen.y - g_screenHeight / 2) * (screen.y - g_screenHeight / 2));
    bool useLOD = screenCenterDist > 300.0f;  // Simplify enemies near edges

    // Simple LOD rendering - just colored circles for distant/edge enemies
    // Always render champions fully (they're special!)
    if (useLOD && e->type != ENEMY_BOSS && e->type != ENEMY_HORNET && !e->isChampion) {
        DrawCircleV(screen, hs * 0.8f, color);
        return;
    }

    // Champion golden pulsing glow effect (drawn behind enemy)
    if (e->isChampion) {
        float glowPulse = sinf(e->championGlow) * 0.3f + 0.7f;  // 0.4 to 1.0 range
        float glowSize = hs * 1.6f * glowPulse;
        Color glowColor = (Color){255, 215, 0, (unsigned char)(80 * glowPulse)};  // Golden glow

        // Outer glow ring
        DrawCircleV(screen, glowSize, glowColor);

        // Inner brighter ring
        glowColor.a = (unsigned char)(60 * glowPulse);
        DrawCircleV(screen, glowSize * 0.7f, glowColor);

        // Affix-specific accent color on the innermost ring
        Color affixColor;
        switch (e->affix) {
            case AFFIX_SWIFT:    affixColor = (Color){100, 200, 255, 100}; break;  // Blue for speed
            case AFFIX_VAMPIRIC: affixColor = (Color){200, 50, 100, 100}; break;   // Red/crimson for vampiric
            case AFFIX_ARMORED:  affixColor = (Color){150, 150, 180, 100}; break;  // Gray/silver for armor
            case AFFIX_SPLITTER: affixColor = (Color){255, 200, 100, 100}; break;  // Orange for splitter
            default:             affixColor = (Color){255, 215, 0, 100}; break;    // Gold default
        }
        DrawCircleV(screen, hs * 0.3f, affixColor);
    }

    switch (e->type) {
        case ENEMY_WALKER:
            // Square
            DrawRectangle((int)(screen.x - hs), (int)(screen.y - hs), (int)e->size, (int)e->size, color);
            break;
        case ENEMY_FAST: {
            // Triangle pointing toward player (use cached angle from UpdateEnemies)
            float angle = e->cachedAngle;
            DrawTriangle(
                (Vector2){screen.x + cosf(angle) * hs, screen.y + sinf(angle) * hs},
                (Vector2){screen.x + cosf(angle - 2.5f) * hs, screen.y + sinf(angle - 2.5f) * hs},
                (Vector2){screen.x + cosf(angle + 2.5f) * hs, screen.y + sinf(angle + 2.5f) * hs}, color);
            break;
        }
        case ENEMY_TANK:
            // Hexagon
            for (int j = 0; j < 6; j++) {
                float a1 = j * PI / 3.0f, a2 = (j + 1) * PI / 3.0f;
                DrawTriangle(screen,
                    (Vector2){screen.x + cosf(a1) * hs, screen.y + sinf(a1) * hs},
                    (Vector2){screen.x + cosf(a2) * hs, screen.y + sinf(a2) * hs}, color);
            }
            break;
        case ENEMY_SWARM:
            // Tiny circle
            DrawCircleV(screen, hs, color);
            break;
        case ENEMY_ELITE: {
            // Diamond with inner glow
            Vector2 pts[4] = {
                {screen.x, screen.y - hs * 1.2f},
                {screen.x + hs, screen.y},
                {screen.x, screen.y + hs * 1.2f},
                {screen.x - hs, screen.y}
            };
            DrawTriangle(pts[0], pts[1], pts[2], color);
            DrawTriangle(pts[0], pts[2], pts[3], color);
            // Inner highlight
            Color inner = {255, 255, 255, 100};
            DrawCircleV(screen, hs * 0.3f, inner);
            break;
        }
        case ENEMY_HORNET: {
            // Wasp/hornet shape - elongated body with stinger
            // Body (elongated oval made of triangles)
            Color bodyColor = color;
            if (e->laserCharging || e->laserFiring) {
                // Flash when charging/firing
                float flash = sinf(g_game.bgTime * 15.0f) * 0.5f + 0.5f;
                bodyColor = (Color){
                    (unsigned char)(color.r + (255 - color.r) * flash * 0.3f),
                    (unsigned char)(color.g + (255 - color.g) * flash * 0.3f),
                    (unsigned char)(color.b + (255 - color.b) * flash * 0.3f),
                    255
                };
            }

            // Main body - horizontal oval
            DrawCircleV(screen, hs * 0.8f, bodyColor);
            // Rear segment (larger)
            DrawCircleV((Vector2){screen.x - hs * 0.5f, screen.y}, hs * 0.6f, bodyColor);
            // Stinger
            Vector2 dir = Normalize((Vector2){g_game.player.pos.x - e->pos.x, g_game.player.pos.y - e->pos.y});
            float facing = atan2f(-dir.y, -dir.x);  // Face away from player (rear toward player)
            DrawTriangle(
                (Vector2){screen.x + cosf(facing) * hs * 1.4f, screen.y + sinf(facing) * hs * 1.4f},
                (Vector2){screen.x + cosf(facing - 0.4f) * hs * 0.6f, screen.y + sinf(facing - 0.4f) * hs * 0.6f},
                (Vector2){screen.x + cosf(facing + 0.4f) * hs * 0.6f, screen.y + sinf(facing + 0.4f) * hs * 0.6f},
                bodyColor);

            // Wings (translucent)
            Color wingColor = {200, 200, 255, 100};
            DrawCircleV((Vector2){screen.x - hs * 0.2f, screen.y - hs * 0.8f}, hs * 0.5f, wingColor);
            DrawCircleV((Vector2){screen.x - hs * 0.2f, screen.y + hs * 0.8f}, hs * 0.5f, wingColor);

            // Stripes (dark bands)
            Color stripe = {40, 30, 0, 255};
            DrawRectangle((int)(screen.x - hs * 0.15f), (int)(screen.y - hs * 0.5f), (int)(hs * 0.15f), (int)(hs), stripe);
            DrawRectangle((int)(screen.x + hs * 0.2f), (int)(screen.y - hs * 0.3f), (int)(hs * 0.1f), (int)(hs * 0.6f), stripe);
            break;
        }
        case ENEMY_BRUTE: {
            // Large octagon
            for (int j = 0; j < 8; j++) {
                float a1 = j * PI / 4.0f, a2 = (j + 1) * PI / 4.0f;
                DrawTriangle(screen,
                    (Vector2){screen.x + cosf(a1) * hs, screen.y + sinf(a1) * hs},
                    (Vector2){screen.x + cosf(a2) * hs, screen.y + sinf(a2) * hs}, color);
            }
            // Darker inner circle for depth
            Color darker = {(unsigned char)(color.r * 0.6f), (unsigned char)(color.g * 0.6f), (unsigned char)(color.b * 0.6f), 255};
            DrawCircleV(screen, hs * 0.5f, darker);
            break;
        }
        case ENEMY_BOSS: {
            // Large star shape with glow
            // Outer glow
            Color glow = color;
            glow.a = 60;
            DrawCircleV(screen, hs * 1.3f, glow);

            // Star points (8-pointed)
            for (int j = 0; j < 8; j++) {
                float a1 = j * PI / 4.0f;
                float a2 = a1 + PI / 8.0f;
                float a3 = a1 + PI / 4.0f;
                Vector2 outer1 = {screen.x + cosf(a1) * hs, screen.y + sinf(a1) * hs};
                Vector2 inner = {screen.x + cosf(a2) * hs * 0.5f, screen.y + sinf(a2) * hs * 0.5f};
                Vector2 outer2 = {screen.x + cosf(a3) * hs, screen.y + sinf(a3) * hs};
                DrawTriangle(screen, outer1, inner, color);
                DrawTriangle(screen, inner, outer2, color);
            }

            // Crown/horn
            DrawTriangle(
                (Vector2){screen.x, screen.y - hs * 1.4f},
                (Vector2){screen.x - hs * 0.3f, screen.y - hs * 0.8f},
                (Vector2){screen.x + hs * 0.3f, screen.y - hs * 0.8f},
                (Color){255, 215, 0, 255}  // Gold crown
            );
            break;
        }
        // =====================================================================
        // BULLET HELL ENEMIES
        // =====================================================================
        case ENEMY_SPINNER: {
            // Rotating gear/wheel with central eye
            Color bodyColor = color;
            // Outer spinning spokes
            for (int s = 0; s < 8; s++) {
                float spokeAngle = e->spinAngle + s * PI / 4.0f;
                Vector2 outer = {screen.x + cosf(spokeAngle) * hs, screen.y + sinf(spokeAngle) * hs};
                Vector2 mid = {screen.x + cosf(spokeAngle) * hs * 0.5f, screen.y + sinf(spokeAngle) * hs * 0.5f};
                DrawLineEx(mid, outer, 3, bodyColor);
                DrawCircleV(outer, 4, bodyColor);
            }
            // Central body
            DrawCircleV(screen, hs * 0.6f, bodyColor);
            // Eye (changes when vulnerable)
            Color eyeColor = e->isVulnerable ? COLOR_SPINNER_EYE : (Color){40, 40, 60, 255};
            float eyeSize = e->isVulnerable ? hs * 0.4f : hs * 0.25f;
            DrawCircleV(screen, eyeSize, eyeColor);
            if (e->isVulnerable) {
                // Pulsing glow when vulnerable
                float pulse = sinf(g_game.bgTime * 10.0f) * 0.3f + 0.7f;
                Color glowColor = COLOR_SPINNER_EYE;
                glowColor.a = (unsigned char)(100 * pulse);
                DrawCircleV(screen, eyeSize * 1.5f, glowColor);
            }
            break;
        }
        case ENEMY_MIRROR: {
            // Crystalline/mirror shape
            Color bodyColor = color;
            // Determine if this should be highlighted (real one during reveal)
            bool isRevealed = !e->isDecoy && e->revealTimer > 0;
            if (isRevealed) {
                bodyColor = COLOR_MIRROR_REAL;
                // Golden glow for real one
                Color glow = COLOR_MIRROR_REAL;
                glow.a = 80;
                DrawCircleV(screen, hs * 1.4f, glow);
            }
            // Diamond shape
            Vector2 pts[4] = {
                {screen.x, screen.y - hs},
                {screen.x + hs * 0.7f, screen.y},
                {screen.x, screen.y + hs},
                {screen.x - hs * 0.7f, screen.y}
            };
            DrawTriangle(pts[0], pts[1], pts[2], bodyColor);
            DrawTriangle(pts[0], pts[2], pts[3], bodyColor);
            // Inner reflection
            Color inner = {255, 255, 255, 80};
            DrawTriangle(
                (Vector2){screen.x, screen.y - hs * 0.5f},
                (Vector2){screen.x + hs * 0.3f, screen.y},
                (Vector2){screen.x - hs * 0.3f, screen.y},
                inner);
            // Decoy indicator (subtle transparency)
            if (e->isDecoy) {
                color.a = 180;
            }
            break;
        }
        case ENEMY_SHIELDER: {
            // Body is hexagon
            for (int j = 0; j < 6; j++) {
                float a1 = j * PI / 3.0f, a2 = (j + 1) * PI / 3.0f;
                DrawTriangle(screen,
                    (Vector2){screen.x + cosf(a1) * hs * 0.8f, screen.y + sinf(a1) * hs * 0.8f},
                    (Vector2){screen.x + cosf(a2) * hs * 0.8f, screen.y + sinf(a2) * hs * 0.8f}, color);
            }
            // Draw rotating shield arc
            float shieldStart = e->shieldAngle - (SHIELDER_SHIELD_ARC / 2.0f) * DEG2RAD;
            float shieldEnd = e->shieldAngle + (SHIELDER_SHIELD_ARC / 2.0f) * DEG2RAD;
            // Shield as thick arc (using line segments)
            // OPTIMIZED: Use 0.2f step instead of 0.1f (half the segments, ~10 draw calls vs ~20)
            Color shieldColor = COLOR_SHIELD;
            if (e->isCharging) {
                shieldColor = (Color){255, 150, 100, 255};  // Orange when charging
            }
            for (float a = shieldStart; a < shieldEnd; a += 0.2f) {
                Vector2 p1 = {screen.x + cosf(a) * hs * 1.1f, screen.y + sinf(a) * hs * 1.1f};
                Vector2 p2 = {screen.x + cosf(a + 0.2f) * hs * 1.1f, screen.y + sinf(a + 0.2f) * hs * 1.1f};
                DrawLineEx(p1, p2, 5, shieldColor);  // Slightly thicker to compensate for fewer segments
            }
            // Shield edge caps
            Vector2 capStart = {screen.x + cosf(shieldStart) * hs * 1.1f, screen.y + sinf(shieldStart) * hs * 1.1f};
            Vector2 capEnd = {screen.x + cosf(shieldEnd) * hs * 1.1f, screen.y + sinf(shieldEnd) * hs * 1.1f};
            DrawCircleV(capStart, 4, shieldColor);
            DrawCircleV(capEnd, 4, shieldColor);
            break;
        }
        case ENEMY_BOMBER: {
            // Round body with danger markings
            Color bodyColor = color;
            if (e->stunnedTimer > 0) {
                // Flash when stunned/vulnerable
                float flash = sinf(g_game.bgTime * 12.0f) * 0.5f + 0.5f;
                bodyColor.r = (unsigned char)(color.r + (255 - color.r) * flash * 0.5f);
            }
            DrawCircleV(screen, hs, bodyColor);
            // Hazard stripes
            Color stripeColor = {40, 40, 40, 255};
            for (int s = 0; s < 3; s++) {
                float stripeY = screen.y - hs * 0.5f + s * hs * 0.5f;
                DrawRectangle((int)(screen.x - hs * 0.6f), (int)(stripeY - 2), (int)(hs * 1.2f), 4, stripeColor);
            }
            // Bomb symbol
            DrawCircleV((Vector2){screen.x, screen.y - hs * 0.2f}, hs * 0.3f, (Color){60, 60, 60, 255});
            // Fuse
            DrawLineEx(
                (Vector2){screen.x, screen.y - hs * 0.5f},
                (Vector2){screen.x + hs * 0.3f, screen.y - hs * 0.8f},
                2, (Color){200, 150, 100, 255});
            break;
        }
        case ENEMY_PHASER: {
            // Ghost-like appearance with variable visibility
            Color bodyColor = color;
            bodyColor.a = (unsigned char)(255 * e->visibility);
            // Wavy ghost shape
            DrawCircleV(screen, hs * 0.9f, bodyColor);
            // Tail wisps
            for (int w = 0; w < 3; w++) {
                float wispAngle = PI / 2.0f + (w - 1) * 0.4f;
                float wispLen = hs * (0.6f + sinf(g_game.bgTime * 3.0f + w) * 0.2f);
                Vector2 wispEnd = {
                    screen.x + cosf(wispAngle) * wispLen,
                    screen.y + sinf(wispAngle) * wispLen
                };
                Color wispColor = bodyColor;
                wispColor.a = (unsigned char)(wispColor.a * 0.6f);
                DrawLineEx(screen, wispEnd, 4, wispColor);
            }
            // Face (only when visible)
            if (e->visibility > 0.5f) {
                Color faceColor = {255, 255, 255, (unsigned char)(200 * e->visibility)};
                DrawCircleV((Vector2){screen.x - hs * 0.25f, screen.y - hs * 0.1f}, 3, faceColor);
                DrawCircleV((Vector2){screen.x + hs * 0.25f, screen.y - hs * 0.1f}, 3, faceColor);
            }
            break;
        }
        default: break;
    }

    // Eyes (size varies by enemy)
    float eyeSize = e->size >= 30 ? 3 : 2;
    float eyeOff = e->size * 0.2f;
    if (e->type != ENEMY_SWARM) {  // Swarm too small for eyes
        DrawCircleV((Vector2){screen.x - eyeOff, screen.y - eyeOff * 0.5f}, eyeSize, COLOR_ENEMY_EYE);
        DrawCircleV((Vector2){screen.x + eyeOff, screen.y - eyeOff * 0.5f}, eyeSize, COLOR_ENEMY_EYE);
    }

    // HP bar for tough enemies
    if ((e->type == ENEMY_BRUTE || e->type == ENEMY_BOSS) && e->hp < e->maxHp) {
        float barWidth = e->size * 1.2f;
        float barHeight = 4;
        float hpPercent = (float)e->hp / e->maxHp;
        DrawRectangle((int)(screen.x - barWidth / 2), (int)(screen.y + hs + 5), (int)barWidth, (int)barHeight, (Color){40, 40, 40, 200});
        DrawRectangle((int)(screen.x - barWidth / 2), (int)(screen.y + hs + 5), (int)(barWidth * hpPercent), (int)barHeight, COLOR_HP_BAR);
    }
}

static void DrawEnemies(void) {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (g_game.enemies[i].active) DrawEnemy(&g_game.enemies[i]);
    }
}

// Draw hornet laser beams (warning lines when charging, solid beams when firing)
static void DrawHornetLasers(void) {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        Enemy *e = &g_game.enemies[i];
        if (!e->active || e->type != ENEMY_HORNET) continue;
        if (!e->laserCharging && !e->laserFiring) continue;

        Vector2 screenStart = WorldToScreen(e->pos);
        float laserLength = 600.0f;
        Vector2 worldEnd = {
            e->pos.x + cosf(e->laserAngle) * laserLength,
            e->pos.y + sinf(e->laserAngle) * laserLength
        };
        Vector2 screenEnd = WorldToScreen(worldEnd);

        if (e->laserCharging) {
            // Warning phase: pulsing dashed line
            float chargeProgress = 1.0f - (e->laserChargeTimer / HORNET_LASER_CHARGE_TIME);
            float pulse = sinf(g_game.bgTime * 12.0f) * 0.5f + 0.5f;

            // Draw dashed warning line
            Color warnColor = COLOR_HORNET_LASER;
            warnColor.a = (unsigned char)(80 + 80 * pulse);

            // Draw segments
            int segments = 20;
            float segLen = 1.0f / segments;
            for (int s = 0; s < segments; s++) {
                // Only draw odd segments (dashed effect)
                if (s % 2 == 0) continue;

                // Increase visible segments as charge progresses
                float visibleProgress = chargeProgress * 1.5f;  // Slight overshoot
                if ((float)s / segments > visibleProgress) continue;

                float t1 = s * segLen;
                float t2 = (s + 1) * segLen;
                Vector2 p1 = {
                    screenStart.x + (screenEnd.x - screenStart.x) * t1,
                    screenStart.y + (screenEnd.y - screenStart.y) * t1
                };
                Vector2 p2 = {
                    screenStart.x + (screenEnd.x - screenStart.x) * t2,
                    screenStart.y + (screenEnd.y - screenStart.y) * t2
                };
                DrawLineEx(p1, p2, 2.0f + pulse, warnColor);
            }

            // Bright dot at hornet when about to fire
            if (chargeProgress > 0.7f) {
                float bright = (chargeProgress - 0.7f) / 0.3f;
                Color brightColor = {255, 200, 150, (unsigned char)(200 * bright)};
                DrawCircleV(screenStart, 8 + pulse * 4, brightColor);
            }
        } else if (e->laserFiring) {
            // Active laser: bright solid beam
            float fireProgress = 1.0f - (e->laserActiveTimer / HORNET_LASER_DURATION);

            // Outer glow
            Color glowColor = COLOR_HORNET_LASER;
            glowColor.a = (unsigned char)(100 * (1.0f - fireProgress * 0.5f));
            DrawLineEx(screenStart, screenEnd, HORNET_LASER_WIDTH * 2.5f, glowColor);

            // Core beam
            Color coreColor = {255, 255, 200, 255};  // Bright yellow-white core
            DrawLineEx(screenStart, screenEnd, HORNET_LASER_WIDTH, coreColor);

            // Inner bright line
            Color innerColor = {255, 255, 255, 255};
            DrawLineEx(screenStart, screenEnd, HORNET_LASER_WIDTH * 0.4f, innerColor);

            // Sparks along the beam
            float sparkTime = g_game.bgTime * 20.0f;
            for (int s = 0; s < 5; s++) {
                float t = fmodf(sparkTime + s * 0.2f, 1.0f);
                Vector2 sparkPos = {
                    screenStart.x + (screenEnd.x - screenStart.x) * t,
                    screenStart.y + (screenEnd.y - screenStart.y) * t
                };
                Color sparkColor = {255, 255, 255, (unsigned char)(150 * (1.0f - t))};
                DrawCircleV(sparkPos, 3, sparkColor);
            }

            // Impact flash at origin
            Color flashColor = {255, 220, 150, (unsigned char)(150 * (1.0f - fireProgress))};
            DrawCircleV(screenStart, 10 + sinf(g_game.bgTime * 30.0f) * 3, flashColor);
        }
    }
}

// Draw enemy bullets (bullet hell patterns)
static void DrawEnemyBullets(void) {
    for (int i = 0; i < MAX_ENEMY_BULLETS; i++) {
        EnemyBullet *b = &g_game.enemyBullets[i];
        if (!b->active) continue;
        if (!IsOnScreen(b->pos, b->size * 2)) continue;

        Vector2 screen = WorldToScreen(b->pos);
        // Outer glow
        Color glow = b->color;
        glow.a = 100;
        DrawCircleV(screen, b->size * 1.5f, glow);
        // Core
        DrawCircleV(screen, b->size, b->color);
        // Bright center
        DrawCircleV(screen, b->size * 0.4f, WHITE);
    }
}

// Draw mines (bomber enemy)
static void DrawMines(void) {
    for (int i = 0; i < MAX_MINES; i++) {
        Mine *m = &g_game.mines[i];
        if (!m->active) continue;
        if (!IsOnScreen(m->pos, m->radius)) continue;

        Vector2 screen = WorldToScreen(m->pos);

        if (m->exploding) {
            // Explosion animation
            float progress = 1.0f - (m->explodeTimer / 0.3f);
            float radius = m->radius * progress;
            Color explodeColor = COLOR_MINE;
            explodeColor.a = (unsigned char)(200 * (1.0f - progress));
            DrawCircleV(screen, radius, explodeColor);
            // Inner bright flash
            Color flashColor = {255, 255, 200, (unsigned char)(255 * (1.0f - progress))};
            DrawCircleV(screen, radius * 0.5f, flashColor);
        } else {
            // Mine about to explode
            float timeLeft = m->timer / BOMBER_MINE_DELAY;
            float pulse = sinf(g_game.bgTime * (10.0f + (1.0f - timeLeft) * 20.0f)) * 0.3f + 0.7f;

            // Warning radius
            Color warnColor = COLOR_MINE;
            warnColor.a = (unsigned char)(40 * (1.0f - timeLeft));
            DrawCircleV(screen, m->radius, warnColor);

            // Mine body
            Color mineColor = COLOR_MINE;
            mineColor.r = (unsigned char)(mineColor.r * pulse + 255 * (1.0f - pulse) * (1.0f - timeLeft));
            DrawCircleV(screen, 8, mineColor);

            // Spikes
            for (int s = 0; s < 8; s++) {
                float spikeAngle = s * PI / 4.0f;
                Vector2 spikeEnd = {
                    screen.x + cosf(spikeAngle) * 12,
                    screen.y + sinf(spikeAngle) * 12
                };
                DrawLineEx(screen, spikeEnd, 2, mineColor);
            }

            // Blinking light
            if (pulse > 0.85f) {
                DrawCircleV(screen, 3, (Color){255, 255, 255, 255});
            }
        }
    }
}

// =============================================================================
// WEAPONS
// =============================================================================

// Get weapon stats based on tier (with synergy bonuses)
static int GetWeaponDamage(WeaponType type) {
    int tier = g_game.weapons[type].tier;
    if (tier <= 0) return 0;
    int baseDmg;
    switch (type) {
        case WEAPON_MELEE: baseDmg = MELEE_BASE_DAMAGE; break;
        case WEAPON_DISTANCE: baseDmg = BULLET_BASE_DAMAGE; break;
        case WEAPON_MAGIC: baseDmg = WAVE_BASE_DAMAGE; break;
        case WEAPON_RADIUS: baseDmg = ORBIT_BASE_DAMAGE; break;
        case WEAPON_MYSTIC: baseDmg = LIGHTNING_BASE_DAMAGE; break;
        case WEAPON_SEEKER: baseDmg = SEEKER_BASE_DAMAGE; break;
        case WEAPON_BOOMERANG: baseDmg = BOOMERANG_BASE_DAMAGE; break;
        case WEAPON_POISON: baseDmg = POISON_BASE_DAMAGE; break;
        case WEAPON_CHAIN: baseDmg = CHAIN_BASE_DAMAGE; break;
        default: baseDmg = 10;
    }
    int damage = baseDmg + (tier - 1) * (baseDmg / 2);

    // Apply synergy damage bonus
    float synDmg, synSpd, synArea;
    int synProj;
    GetSynergyBonuses(type, &synDmg, &synSpd, &synArea, &synProj);
    float totalMult = synDmg;

    // Apply class weapon bonus if this is the preferred weapon
    const ClassStats *cls = &CLASS_STATS[g_game.player.playerClass];
    if (type == cls->preferredWeapon && cls->weaponDamageBonus > 0) {
        totalMult *= (1.0f + cls->weaponDamageBonus / 100.0f);
    }

    return (int)(damage * totalMult);
}

static float GetWeaponCooldown(WeaponType type) {
    int tier = g_game.weapons[type].tier;
    if (tier <= 0) return 999.0f;
    float baseCd;
    switch (type) {
        case WEAPON_MELEE: baseCd = MELEE_BASE_COOLDOWN; break;
        case WEAPON_DISTANCE: baseCd = BULLET_BASE_COOLDOWN; break;
        case WEAPON_MAGIC: baseCd = WAVE_BASE_COOLDOWN; break;
        case WEAPON_RADIUS: baseCd = 0.0f; break;
        case WEAPON_MYSTIC: baseCd = LIGHTNING_BASE_COOLDOWN; break;
        case WEAPON_SEEKER: baseCd = SEEKER_BASE_COOLDOWN; break;
        case WEAPON_BOOMERANG: baseCd = BOOMERANG_BASE_COOLDOWN; break;
        case WEAPON_POISON: baseCd = POISON_BASE_COOLDOWN; break;
        case WEAPON_CHAIN: baseCd = CHAIN_BASE_COOLDOWN; break;
        default: baseCd = 1.0f;
    }
    // Apply tier reduction and attack speed multiplier
    float cd = baseCd * (1.0f - (tier - 1) * 0.1f);
    cd *= GetAttackSpeedMultiplier();

    // Apply synergy speed bonus
    float synDmg, synSpd, synArea;
    int synProj;
    GetSynergyBonuses(type, &synDmg, &synSpd, &synArea, &synProj);
    return cd * synSpd;
}

// Melee weapon
static void TriggerMelee(void) {
    WeaponSkill *skill = &g_game.weapons[WEAPON_MELEE];
    MeleeSwing *m = &g_game.melee;
    int tier = skill->tier;
    int bt = skill->branchTier;

    // Check for spin branch - continuous spinning
    if (skill->branch == MELEE_BRANCH_SPIN && bt >= 5) {
        // Tier 5: Always spinning
        skill->spinning = true;
    }

    m->active = true;
    m->timer = 0;
    m->duration = 0.15f;
    m->angle = g_game.player.angle;
    m->damage = GetWeaponDamage(WEAPON_MELEE);
    m->range = (MELEE_BASE_RANGE + tier * 10) * GetAreaMultiplier();

    // Branch-specific arc modifications
    float arcDegrees = MELEE_BASE_ARC + tier * 15;
    if (skill->branch == MELEE_BRANCH_WIDE) {
        // Wide Arc branch: significantly wider arc
        arcDegrees += bt * 45;  // +45, +90, +135, +180, +225 degrees
        if (arcDegrees > 360) arcDegrees = 360;
    } else if (skill->branch == MELEE_BRANCH_SPIN) {
        // Spin branch: 360 degree attack
        arcDegrees = 360;
    }
    m->arc = arcDegrees * DEG2RAD * GetAreaMultiplier();

    // Power branch: damage multiplier
    if (skill->branch == MELEE_BRANCH_POWER) {
        float powerMult = 1.0f + bt * 0.5f;  // 1.5x, 2x, 2.5x, 3x, 3.5x
        m->damage = (int)(m->damage * powerMult);
    }
}

static void UpdateMelee(float dt) {
    WeaponSkill *skill = &g_game.weapons[WEAPON_MELEE];
    MeleeSwing *m = &g_game.melee;

    // Handle spin branch continuous spinning
    if (skill->branch == MELEE_BRANCH_SPIN && skill->spinning) {
        skill->spinTimer += dt;
        float spinDuration = 0.5f + skill->branchTier * 0.5f;  // 0.5s to 2.5s
        if (skill->branchTier >= 5) spinDuration = 999.0f;  // Always spin at max tier

        // Continuous damage in 360 degrees while spinning - use spatial grid
        float spinDmg = GetWeaponDamage(WEAPON_MELEE) * 0.3f;  // Reduced per-tick damage
        float spinRange = (MELEE_BASE_RANGE + skill->tier * 10) * GetAreaMultiplier();
        float spinRangeSq = spinRange * spinRange;

        // Use spatial grid to find enemies in range
        int hitList[32];
        int hitCount = 0;
        CheckEnemyCollisionAtPoint(g_game.player.pos, spinRange, hitList, 32, &hitCount);

        for (int h = 0; h < hitCount; h++) {
            Enemy *e = &g_game.enemies[hitList[h]];
            if (!e->active) continue;
            // Use squared distance for final check (grid radius is approximate)
            float dx = g_game.player.pos.x - e->pos.x;
            float dy = g_game.player.pos.y - e->pos.y;
            if (dx * dx + dy * dy < spinRangeSq) {
                DamageEnemy(e, (int)spinDmg);
            }
        }

        if (skill->spinTimer >= spinDuration && skill->branchTier < 5) {
            skill->spinning = false;
            skill->spinTimer = 0;
        }
    }

    if (!m->active) return;

    m->timer += dt;
    Player *player = &g_game.player;
    float rangeSq = m->range * m->range;

    // Use spatial grid to find enemies in melee range
    int hitList[32];
    int hitCount = 0;
    CheckEnemyCollisionAtPoint(player->pos, m->range, hitList, 32, &hitCount);

    for (int h = 0; h < hitCount; h++) {
        Enemy *e = &g_game.enemies[hitList[h]];
        if (!e->active) continue;

        // Use squared distance for range check
        float dx = player->pos.x - e->pos.x;
        float dy = player->pos.y - e->pos.y;
        float distSq = dx * dx + dy * dy;
        if (distSq > rangeSq) continue;

        float angleToEnemy = atan2f(e->pos.y - player->pos.y, e->pos.x - player->pos.x);
        if (fabsf(AngleDiff(m->angle, angleToEnemy)) < m->arc / 2) {
            DamageEnemy(e, m->damage);

            // Power branch: knockback
            if (skill->branch == MELEE_BRANCH_POWER && skill->branchTier >= 2) {
                Vector2 knockDir = Normalize((Vector2){e->pos.x - player->pos.x, e->pos.y - player->pos.y});
                float knockForce = 30.0f + skill->branchTier * 15.0f;
                e->pos.x += knockDir.x * knockForce;
                e->pos.y += knockDir.y * knockForce;
            }
        }
    }

    if (m->timer >= m->duration) m->active = false;
}

static void DrawMelee(void) {
    WeaponSkill *skill = &g_game.weapons[WEAPON_MELEE];
    MeleeSwing *m = &g_game.melee;
    Vector2 playerScreen = WorldToScreen(g_game.player.pos);

    // Draw spin effect
    if (skill->branch == MELEE_BRANCH_SPIN && skill->spinning) {
        float spinRange = (MELEE_BASE_RANGE + skill->tier * 10) * GetAreaMultiplier();
        float spinAngle = g_game.bgTime * 15.0f;  // Fast rotation

        Color spinColor = COLOR_MELEE;
        spinColor.a = 150;

        // Draw spinning blades
        for (int i = 0; i < 4; i++) {
            float a = spinAngle + i * PI / 2;
            Vector2 tip = {playerScreen.x + cosf(a) * spinRange, playerScreen.y + sinf(a) * spinRange};
            DrawLineEx(playerScreen, tip, 4, spinColor);
        }

        // Draw spin radius indicator
        spinColor.a = 50;
        DrawCircleLines((int)playerScreen.x, (int)playerScreen.y, spinRange, spinColor);
    }

    if (!m->active) return;

    float progress = m->timer / m->duration;
    float alpha = 1.0f - progress;
    Color c = COLOR_MELEE;

    // Power branch: red tint
    if (skill->branch == MELEE_BRANCH_POWER) {
        c = (Color){255, 80, 80, 255};
    }
    c.a = (unsigned char)(200 * alpha);

    float sweepAngle = m->angle - m->arc / 2 + m->arc * progress;
    int arcLines = (int)(8 * (m->arc / (PI / 2)));  // More lines for wider arcs
    if (arcLines < 4) arcLines = 4;
    if (arcLines > 24) arcLines = 24;

    for (int i = 0; i < arcLines; i++) {
        float a = m->angle - m->arc / 2 + m->arc * i / (arcLines - 1);
        Vector2 tip = {playerScreen.x + cosf(a) * m->range, playerScreen.y + sinf(a) * m->range};
        DrawLineEx(playerScreen, tip, 3 * alpha, c);
    }
}

// Distance weapon (bullets)
static void FireBullet(void) {
    Player *player = &g_game.player;
    WeaponSkill *skill = &g_game.weapons[WEAPON_DISTANCE];
    int tier = skill->tier;
    int bt = skill->branchTier;

    int bulletCount = 1 + GetBonusProjectiles();
    float spreadAngle = 0.15f;

    // Branch modifications
    if (skill->branch == DISTANCE_BRANCH_RAPID) {
        // Rapid fire: more bullets
        bulletCount += bt;  // +1 to +5 extra bullets
    } else if (skill->branch == DISTANCE_BRANCH_SPREAD) {
        // Spread shot: shotgun pattern
        int spreadCounts[] = {3, 5, 7, 9, 12};
        bulletCount = spreadCounts[bt > 0 ? bt - 1 : 0];
        spreadAngle = (bt < 3) ? 0.8f : 0.6f;  // Tighter at higher tiers
        if (bt >= 5) spreadAngle = PI * 2;  // Nova at max
    }

    int fired = 0;
    for (int b = 0; b < bulletCount && fired < bulletCount; b++) {
        float angleOffset;
        if (skill->branch == DISTANCE_BRANCH_SPREAD && bt >= 5) {
            // Nova: 360 degree spread
            angleOffset = (b * PI * 2) / bulletCount;
        } else if (bulletCount > 1) {
            angleOffset = (b - (bulletCount - 1) / 2.0f) * spreadAngle / (bulletCount - 1);
        } else {
            angleOffset = 0;
        }
        float bulletAngle = player->angle + angleOffset;

        for (int i = 0; i < MAX_PROJECTILES; i++) {
            Projectile *p = &g_game.projectiles[i];
            if (!p->active) {
                p->pos = player->pos;
                p->vel = (Vector2){cosf(bulletAngle) * BULLET_SPEED, sinf(bulletAngle) * BULLET_SPEED};
                p->size = (BULLET_SIZE + tier) * GetAreaMultiplier();
                p->damage = GetWeaponDamage(WEAPON_DISTANCE);
                p->active = true;
                p->lifetime = 2.0f;
                fired++;
                break;
            }
        }
    }
}

static void UpdateProjectiles(float dt) {
    WeaponSkill *skill = &g_game.weapons[WEAPON_DISTANCE];
    int pierceCount = (skill->branch == DISTANCE_BRANCH_PIERCE) ? skill->pierceCount : 0;
    float pierceDmgBonus = (skill->branch == DISTANCE_BRANCH_PIERCE) ? (1.0f + skill->branchTier * 0.2f) : 1.0f;

    for (int i = 0; i < MAX_PROJECTILES; i++) {
        Projectile *p = &g_game.projectiles[i];
        if (!p->active) continue;

        p->pos.x += p->vel.x * dt;
        p->pos.y += p->vel.y * dt;
        p->lifetime -= dt;

        if (p->lifetime <= 0 ||
            p->pos.x < WORLD_PADDING - 50 || p->pos.x > WORLD_WIDTH - WORLD_PADDING + 50 ||
            p->pos.y < WORLD_PADDING - 50 || p->pos.y > WORLD_HEIGHT - WORLD_PADDING + 50) {
            p->active = false;
            continue;
        }

        // Use spatial grid for collision (O(1) instead of O(n))
        int hitList[16];
        int hitCount = 0;
        CheckEnemyCollisionAtPoint(p->pos, p->size, hitList, 16, &hitCount);

        for (int h = 0; h < hitCount; h++) {
            Enemy *e = &g_game.enemies[hitList[h]];
            if (!e->active) continue;

            int dmg = (int)(p->damage * pierceDmgBonus);
            DamageEnemy(e, dmg);

            if (skill->branch == DISTANCE_BRANCH_PIERCE) {
                // Pierce through enemies
                if (skill->branchTier >= 4 || h < pierceCount - 1) {
                    // Continue piercing (tier 4+ = infinite pierce)
                    SpawnParticleBurst(p->pos, 2, COLOR_BULLET, 40, 2);
                    continue;
                }
            }
            p->active = false;
            break;
        }
    }
}

static void DrawProjectiles(void) {
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        Projectile *p = &g_game.projectiles[i];
        if (!p->active || !IsOnScreen(p->pos, 20)) continue;

        Vector2 screen = WorldToScreen(p->pos);
        float angle = atan2f(p->vel.y, p->vel.x);
        Vector2 tail = {screen.x - cosf(angle) * p->size * 2, screen.y - sinf(angle) * p->size * 2};
        DrawLineEx(tail, screen, p->size * 0.6f, (Color){COLOR_BULLET.r, COLOR_BULLET.g, COLOR_BULLET.b, 100});
        DrawCircleV(screen, p->size / 2, COLOR_BULLET);
    }
}

// Magic weapon (wave)
static void TriggerWave(void) {
    WeaponSkill *skill = &g_game.weapons[WEAPON_MAGIC];
    WaveAttack *w = &g_game.wave;
    int tier = skill->tier;
    int bt = skill->branchTier;

    w->active = true;
    w->radius = 0;
    w->maxRadius = (WAVE_BASE_RADIUS + tier * 20) * GetAreaMultiplier();
    w->timer = 0;
    w->duration = WAVE_DURATION;
    w->damage = GetWeaponDamage(WEAPON_MAGIC);

    // Branch modifications
    if (skill->branch == MAGIC_BRANCH_NOVA) {
        // Nova: much larger radius
        w->maxRadius *= (1.0f + bt * 0.5f);  // +50% to +250%
        w->duration *= 1.2f;  // Slower expansion
    } else if (skill->branch == MAGIC_BRANCH_PULSE) {
        // Pulse: smaller but faster waves (handled in update)
        w->maxRadius *= 0.6f;
        w->duration *= 0.5f;  // Faster
    } else if (skill->branch == MAGIC_BRANCH_FREEZE) {
        // Freeze: applies slow/freeze effect
        w->maxRadius *= 1.2f;
    }
}

static void UpdateWave(float dt) {
    WaveAttack *w = &g_game.wave;
    if (!w->active) return;

    WeaponSkill *skill = &g_game.weapons[WEAPON_MAGIC];
    w->timer += dt;
    w->radius = (w->timer / w->duration) * w->maxRadius;

    for (int i = 0; i < MAX_ENEMIES; i++) {
        Enemy *e = &g_game.enemies[i];
        if (!e->active) continue;
        float dist = Distance(g_game.player.pos, e->pos);
        if (dist > w->radius - 20 && dist < w->radius + 20) {
            DamageEnemy(e, w->damage);

            // Freeze branch: slow enemies hit by the wave
            if (skill->branch == MAGIC_BRANCH_FREEZE) {
                float slowPercent = skill->freezeAmount;  // 30 to 80% slow
                e->speed *= (1.0f - slowPercent / 100.0f);

                // Higher tiers: spawn frost particles
                if (skill->branchTier >= 2) {
                    Color frostColor = (Color){150, 200, 255, 200};
                    SpawnParticleBurst(e->pos, 3, frostColor, 40, 2);
                }
            }
        }
    }

    if (w->timer >= w->duration) w->active = false;
}

static void DrawWave(void) {
    if (!g_game.wave.active) return;
    Vector2 ps = WorldToScreen(g_game.player.pos);
    float alpha = 1.0f - (g_game.wave.timer / g_game.wave.duration);
    Color c = COLOR_WAVE; c.a = (unsigned char)(c.a * alpha);
    DrawCircleLines((int)ps.x, (int)ps.y, g_game.wave.radius, c);
    DrawCircleLines((int)ps.x, (int)ps.y, g_game.wave.radius - 3, c);
}

// Radius weapon (orbit)
static void UpdateOrbit(float dt) {
    WeaponSkill *skill = &g_game.weapons[WEAPON_RADIUS];
    int tier = skill->tier;
    if (tier <= 0) return;

    Player *player = &g_game.player;
    int bt = skill->branchTier;

    // Branch-specific orb count
    int numOrbs = ORBIT_BASE_COUNT + tier - 1 + GetBonusProjectiles();
    float radius = (ORBIT_BASE_RADIUS + tier * 10) * GetAreaMultiplier();
    float speed = ORBIT_SPEED + tier * 0.3f;
    int damage = GetWeaponDamage(WEAPON_RADIUS);
    float orbSize = ORBIT_SIZE * GetAreaMultiplier();

    if (skill->branch == RADIUS_BRANCH_SWARM) {
        // Swarm: many small fast orbs
        int swarmCounts[] = {3, 5, 7, 10, 12, 20};
        numOrbs += swarmCounts[bt];  // +3 to +20 extra orbs
        orbSize *= 0.6f;  // Smaller orbs
        speed *= (1.3f + bt * 0.15f);  // Faster rotation
        damage = (int)(damage * 0.5f);  // Less damage per orb
    } else if (skill->branch == RADIUS_BRANCH_HEAVY) {
        // Heavy: fewer large slow orbs
        numOrbs = 2 + (bt >= 3 ? 1 : 0);  // 2-3 orbs
        orbSize *= (1.5f + bt * 0.3f);  // Much larger
        speed *= 0.6f;  // Slower rotation
        radius *= 1.3f;  // Wider orbit
        damage = (int)(damage * (1.5f + bt * 0.4f));  // Much more damage
    } else if (skill->branch == RADIUS_BRANCH_SHIELD) {
        // Guardian: defensive orbs that can block attacks
        orbSize *= 1.2f;
        radius *= 0.8f;  // Closer to player
        // Damage is lower for defensive build
        damage = (int)(damage * 0.7f);
    }

    if (numOrbs > MAX_ORBIT_ORBS) numOrbs = MAX_ORBIT_ORBS;

    for (int i = 0; i < numOrbs; i++) {
        OrbitOrb *orb = &g_game.orbitOrbs[i];
        orb->active = true;
        orb->angle += speed * dt;
        orb->damage = damage;

        Vector2 orbPos = {
            player->pos.x + cosf(orb->angle + (i * PI * 2 / numOrbs)) * radius,
            player->pos.y + sinf(orb->angle + (i * PI * 2 / numOrbs)) * radius
        };

        // Enemy collision using spatial grid
        int hitList[8];
        int hitCount = 0;
        CheckEnemyCollisionAtPoint(orbPos, orbSize, hitList, 8, &hitCount);

        for (int h = 0; h < hitCount; h++) {
            Enemy *e = &g_game.enemies[hitList[h]];
            if (!e->active) continue;
            DamageEnemy(e, damage);

            // Swarm branch: tracking at higher tiers
            if (skill->branch == RADIUS_BRANCH_SWARM && bt >= 4) {
                // Slightly pull orbs toward enemies
                float pull = 0.05f;
                orb->angle += (atan2f(e->pos.y - player->pos.y, e->pos.x - player->pos.x) - orb->angle) * pull;
            }
        }
    }

    // Deactivate extra orbs
    for (int i = numOrbs; i < MAX_ORBIT_ORBS; i++) {
        g_game.orbitOrbs[i].active = false;
    }
}

static void DrawOrbit(void) {
    WeaponSkill *skill = &g_game.weapons[WEAPON_RADIUS];
    int tier = skill->tier;
    if (tier <= 0) return;

    Player *player = &g_game.player;
    int bt = skill->branchTier;

    // Calculate display parameters based on branch
    int numOrbs = ORBIT_BASE_COUNT + tier - 1;
    float radius = ORBIT_BASE_RADIUS + tier * 10;
    float size = ORBIT_SIZE + tier;
    Color orbColor = COLOR_ORBIT;

    if (skill->branch == RADIUS_BRANCH_SWARM) {
        int swarmCounts[] = {3, 5, 7, 10, 12, 20};
        numOrbs += swarmCounts[bt];
        size *= 0.6f;
        orbColor = (Color){180, 255, 180, 255};  // Green tint for swarm
    } else if (skill->branch == RADIUS_BRANCH_HEAVY) {
        numOrbs = 2 + (bt >= 3 ? 1 : 0);
        size *= (1.5f + bt * 0.3f);
        radius *= 1.3f;
        orbColor = (Color){100, 100, 255, 255};  // Deep blue for heavy
    } else if (skill->branch == RADIUS_BRANCH_SHIELD) {
        size *= 1.2f;
        radius *= 0.8f;
        orbColor = (Color){255, 220, 100, 255};  // Gold for shield
    }

    if (numOrbs > MAX_ORBIT_ORBS) numOrbs = MAX_ORBIT_ORBS;

    for (int i = 0; i < numOrbs; i++) {
        OrbitOrb *orb = &g_game.orbitOrbs[i];
        if (!orb->active) continue;

        Vector2 orbWorld = {
            player->pos.x + cosf(orb->angle + (i * PI * 2 / numOrbs)) * radius,
            player->pos.y + sinf(orb->angle + (i * PI * 2 / numOrbs)) * radius
        };
        Vector2 orbScreen = WorldToScreen(orbWorld);

        // Draw glow
        DrawCircleV(orbScreen, size + 2, (Color){orbColor.r, orbColor.g, orbColor.b, 80});

        // Branch-specific visuals
        if (skill->branch == RADIUS_BRANCH_HEAVY) {
            // Heavy: spiky appearance
            DrawCircleV(orbScreen, size, orbColor);
            for (int spike = 0; spike < 6; spike++) {
                float spikeAngle = orb->angle * 3 + spike * PI / 3;
                Vector2 spikeEnd = {
                    orbScreen.x + cosf(spikeAngle) * size * 1.4f,
                    orbScreen.y + sinf(spikeAngle) * size * 1.4f
                };
                DrawLineEx(orbScreen, spikeEnd, 2, orbColor);
            }
        } else if (skill->branch == RADIUS_BRANCH_SHIELD) {
            // Shield: ring with inner glow
            DrawCircleLines((int)orbScreen.x, (int)orbScreen.y, size, orbColor);
            DrawCircleV(orbScreen, size * 0.6f, (Color){orbColor.r, orbColor.g, orbColor.b, 150});
        } else {
            // Default/Swarm: solid circle
            DrawCircleV(orbScreen, size, orbColor);
        }
    }
}

// Mystic weapon (lightning)
static void TriggerLightning(void) {
    WeaponSkill *skill = &g_game.weapons[WEAPON_MYSTIC];
    int tier = skill->tier;
    int bt = skill->branchTier;
    int baseDamage = GetWeaponDamage(WEAPON_MYSTIC);

    // Branch-specific behavior
    if (skill->branch == MYSTIC_BRANCH_SMITE) {
        // Smite: Single massive strike on closest enemy
        int targetIdx = FindNearestEnemyGrid(g_game.player.pos, LIGHTNING_RANGE * 1.5f);
        if (targetIdx >= 0) {
            Enemy *target = &g_game.enemies[targetIdx];
            float smiteMult = 2.0f + bt * 0.8f;  // 2.8x to 6x damage
            int smiteDamage = (int)(baseDamage * smiteMult);

            for (int i = 0; i < MAX_LIGHTNING_STRIKES; i++) {
                LightningStrike *l = &g_game.lightning[i];
                if (!l->active) {
                    l->pos = target->pos;
                    l->timer = 0.5f + bt * 0.1f;  // Longer duration
                    l->damage = smiteDamage;
                    l->active = true;
                    DamageEnemy(target, smiteDamage);

                    // Big impact effect
                    SpawnParticleBurst(target->pos, 12 + bt * 2, COLOR_LIGHTNING, 150, 6);
                    g_game.screenShake = fmaxf(g_game.screenShake, 3.0f + bt);
                    break;
                }
            }
        }
        return;
    }

    if (skill->branch == MYSTIC_BRANCH_STORM) {
        // Storm: Many random strikes in area
        int stormCounts[] = {2, 3, 5, 8, 10, 15};
        int numStrikes = stormCounts[bt] + GetBonusProjectiles();
        float stormRange = LIGHTNING_RANGE * (1.0f + bt * 0.2f);

        for (int s = 0; s < numStrikes; s++) {
            // Random position in range
            float angle = RandomFloat(0, PI * 2);
            float dist = RandomFloat(30, stormRange);
            Vector2 strikePos = {
                g_game.player.pos.x + cosf(angle) * dist,
                g_game.player.pos.y + sinf(angle) * dist
            };

            for (int i = 0; i < MAX_LIGHTNING_STRIKES; i++) {
                LightningStrike *l = &g_game.lightning[i];
                if (!l->active) {
                    l->pos = strikePos;
                    l->timer = 0.2f;
                    l->damage = (int)(baseDamage * 0.6f);  // Less damage per strike
                    l->active = true;

                    // Damage enemies near strike
                    float strikeRadius = 25.0f + bt * 5;
                    for (int j = 0; j < MAX_ENEMIES; j++) {
                        Enemy *e = &g_game.enemies[j];
                        if (e->active && Distance(strikePos, e->pos) < strikeRadius) {
                            DamageEnemy(e, l->damage);
                        }
                    }
                    SpawnParticleBurst(strikePos, 4, COLOR_LIGHTNING, 60, 3);
                    break;
                }
            }
        }
        return;
    }

    // Default behavior or Chain branch
    int numStrikes = LIGHTNING_BASE_STRIKES + tier - 1 + GetBonusProjectiles();
    int chainJumps = (skill->branch == MYSTIC_BRANCH_CHAIN) ? skill->chainJumps : 0;

    for (int s = 0; s < numStrikes; s++) {
        // Find random enemy in range
        Enemy *targets[MAX_ENEMIES];
        int numTargets = 0;
        for (int i = 0; i < MAX_ENEMIES; i++) {
            Enemy *e = &g_game.enemies[i];
            if (e->active && Distance(g_game.player.pos, e->pos) < LIGHTNING_RANGE) {
                targets[numTargets++] = e;
            }
        }

        if (numTargets > 0) {
            Enemy *target = targets[GetRandomValue(0, numTargets - 1)];
            int currentDamage = baseDamage;

            for (int i = 0; i < MAX_LIGHTNING_STRIKES; i++) {
                LightningStrike *l = &g_game.lightning[i];
                if (!l->active) {
                    l->pos = target->pos;
                    l->timer = 0.3f;
                    l->damage = currentDamage;
                    l->active = true;
                    DamageEnemy(target, currentDamage);
                    SpawnParticleBurst(target->pos, 6, COLOR_LIGHTNING, 100, 4);
                    break;
                }
            }

            // Chain branch: bounce to nearby enemies
            if (skill->branch == MYSTIC_BRANCH_CHAIN && chainJumps > 0) {
                int hitEnemies[16] = {-1};
                int hitCount = 1;
                hitEnemies[0] = (int)(target - g_game.enemies);
                Vector2 lastPos = target->pos;
                float chainRange = CHAIN_JUMP_RANGE + bt * 15;
                float damageDecay = 0.85f;

                for (int jump = 0; jump < chainJumps; jump++) {
                    // Find closest enemy not already hit
                    int nextTarget = -1;
                    float nearestDist = chainRange;

                    for (int j = 0; j < MAX_ENEMIES; j++) {
                        if (!g_game.enemies[j].active) continue;

                        // Check if already hit
                        bool alreadyHit = false;
                        for (int h = 0; h < hitCount; h++) {
                            if (hitEnemies[h] == j) { alreadyHit = true; break; }
                        }
                        if (alreadyHit) continue;

                        float dist = Distance(lastPos, g_game.enemies[j].pos);
                        if (dist < nearestDist) {
                            nearestDist = dist;
                            nextTarget = j;
                        }
                    }

                    if (nextTarget >= 0 && hitCount < 16) {
                        hitEnemies[hitCount++] = nextTarget;
                        currentDamage = (int)(currentDamage * damageDecay);
                        Enemy *chainTarget = &g_game.enemies[nextTarget];

                        for (int i = 0; i < MAX_LIGHTNING_STRIKES; i++) {
                            LightningStrike *l = &g_game.lightning[i];
                            if (!l->active) {
                                l->pos = chainTarget->pos;
                                l->timer = 0.25f;
                                l->damage = currentDamage;
                                l->active = true;
                                DamageEnemy(chainTarget, currentDamage);
                                SpawnParticleBurst(chainTarget->pos, 4, COLOR_LIGHTNING, 70, 3);
                                break;
                            }
                        }
                        lastPos = chainTarget->pos;
                    } else {
                        break;  // No more valid targets
                    }
                }
            }
        }
    }
}

static void UpdateLightning(float dt) {
    for (int i = 0; i < MAX_LIGHTNING_STRIKES; i++) {
        LightningStrike *l = &g_game.lightning[i];
        if (l->active) {
            l->timer -= dt;
            if (l->timer <= 0) l->active = false;
        }
    }
}

static void DrawLightning(void) {
    for (int i = 0; i < MAX_LIGHTNING_STRIKES; i++) {
        LightningStrike *l = &g_game.lightning[i];
        if (!l->active || !IsOnScreen(l->pos, 50)) continue;

        Vector2 screen = WorldToScreen(l->pos);
        float alpha = l->timer / 0.3f;
        Color c = COLOR_LIGHTNING; c.a = (unsigned char)(255 * alpha);

        // Draw lightning bolt
        DrawLineEx((Vector2){screen.x, screen.y - 60}, (Vector2){screen.x - 5, screen.y - 30}, 3, c);
        DrawLineEx((Vector2){screen.x - 5, screen.y - 30}, (Vector2){screen.x + 5, screen.y - 15}, 3, c);
        DrawLineEx((Vector2){screen.x + 5, screen.y - 15}, (Vector2){screen.x, screen.y}, 3, c);
        DrawCircleV(screen, 8 * alpha, c);
    }
}

// =============================================================================
// SEEKER WEAPON (Homing Missiles)
// =============================================================================

static void FireSeeker(void) {
    int tier = g_game.weapons[WEAPON_SEEKER].tier;
    int missileCount = 1 + (tier > 2 ? 1 : 0) + (tier > 4 ? 1 : 0);

    for (int m = 0; m < missileCount; m++) {
        int targetIdx = FindNearestEnemyGrid(g_game.player.pos, SEEKER_RANGE);
        if (targetIdx < 0) return;

        for (int i = 0; i < MAX_SEEKERS; i++) {
            SeekerMissile *s = &g_game.seekers[i];
            if (!s->active) {
                s->pos = g_game.player.pos;
                s->angle = g_game.player.angle + RandomFloat(-0.3f, 0.3f);
                s->vel = (Vector2){cosf(s->angle) * SEEKER_SPEED, sinf(s->angle) * SEEKER_SPEED};
                s->targetIdx = targetIdx;
                s->damage = GetWeaponDamage(WEAPON_SEEKER);
                s->lifetime = 3.0f;
                s->active = true;
                break;
            }
        }
    }
}

static void UpdateSeekers(float dt) {
    int tier = g_game.weapons[WEAPON_SEEKER].tier;
    float turnRate = SEEKER_TURN_RATE + tier * 0.3f;
    float explosionRadius = (SEEKER_EXPLOSION_RADIUS + tier * 5) * GetAreaMultiplier();

    for (int i = 0; i < MAX_SEEKERS; i++) {
        SeekerMissile *s = &g_game.seekers[i];
        if (!s->active) continue;

        s->lifetime -= dt;
        if (s->lifetime <= 0) { s->active = false; continue; }

        // Retarget if target is dead
        if (s->targetIdx < 0 || !g_game.enemies[s->targetIdx].active) {
            s->targetIdx = FindNearestEnemyGrid(s->pos, SEEKER_RANGE * 2);
        }

        // Homing behavior
        if (s->targetIdx >= 0) {
            Enemy *target = &g_game.enemies[s->targetIdx];
            float targetAngle = atan2f(target->pos.y - s->pos.y, target->pos.x - s->pos.x);
            float angleDiff = AngleDiff(s->angle, targetAngle);
            float maxTurn = turnRate * dt;
            if (fabsf(angleDiff) < maxTurn) {
                s->angle = targetAngle;
            } else {
                s->angle += (angleDiff > 0 ? maxTurn : -maxTurn);
            }
        }

        s->vel = (Vector2){cosf(s->angle) * SEEKER_SPEED, sinf(s->angle) * SEEKER_SPEED};
        s->pos.x += s->vel.x * dt;
        s->pos.y += s->vel.y * dt;

        // Collision with enemies using spatial grid
        int directHit = FindEnemyInRange(s->pos, 8);
        if (directHit >= 0) {
            Enemy *e = &g_game.enemies[directHit];
            // Direct hit
            DamageEnemy(e, s->damage);
            // Explosion AoE using spatial grid
            int aoeHits[16];
            int aoeCount = 0;
            CheckEnemyCollisionAtPoint(s->pos, explosionRadius, aoeHits, 16, &aoeCount);
            for (int k = 0; k < aoeCount; k++) {
                if (aoeHits[k] == directHit || !g_game.enemies[aoeHits[k]].active) continue;
                DamageEnemy(&g_game.enemies[aoeHits[k]], s->damage / 2);
            }
            SpawnParticleBurst(s->pos, 8, COLOR_SEEKER, 100, 5);
            s->active = false;
        }
    }
}

static void DrawSeekers(void) {
    for (int i = 0; i < MAX_SEEKERS; i++) {
        SeekerMissile *s = &g_game.seekers[i];
        if (!s->active || !IsOnScreen(s->pos, 30)) continue;

        Vector2 screen = WorldToScreen(s->pos);

        // Trail
        Vector2 tail = {screen.x - cosf(s->angle) * 12, screen.y - sinf(s->angle) * 12};
        DrawLineEx(tail, screen, 3, (Color){COLOR_SEEKER.r, COLOR_SEEKER.g, COLOR_SEEKER.b, 100});

        // Missile body (triangle)
        float sz = 6;
        Vector2 tip = {screen.x + cosf(s->angle) * sz, screen.y + sinf(s->angle) * sz};
        Vector2 l = {screen.x + cosf(s->angle - 2.5f) * sz, screen.y + sinf(s->angle - 2.5f) * sz};
        Vector2 r = {screen.x + cosf(s->angle + 2.5f) * sz, screen.y + sinf(s->angle + 2.5f) * sz};
        DrawTriangle(tip, r, l, COLOR_SEEKER);

        // Glow
        DrawCircleGradient((int)screen.x, (int)screen.y, 10, (Color){COLOR_SEEKER.r, COLOR_SEEKER.g, COLOR_SEEKER.b, 60}, BLANK);
    }
}

// =============================================================================
// BOOMERANG WEAPON
// =============================================================================

static void FireBoomerang(void) {
    int tier = g_game.weapons[WEAPON_BOOMERANG].tier;

    for (int i = 0; i < MAX_BOOMERANGS; i++) {
        BoomerangProj *b = &g_game.boomerangs[i];
        if (!b->active) {
            b->pos = g_game.player.pos;
            b->startPos = g_game.player.pos;
            b->angle = g_game.player.angle;
            b->spinAngle = 0;
            b->outwardDist = 0;
            b->maxDist = (BOOMERANG_RANGE + tier * 30) * GetAreaMultiplier();
            b->damage = GetWeaponDamage(WEAPON_BOOMERANG);
            b->size = (BOOMERANG_SIZE + tier * 2) * GetAreaMultiplier();
            b->returning = false;
            b->active = true;
            return;
        }
    }
}

static void UpdateBoomerangs(float dt) {
    int tier = g_game.weapons[WEAPON_BOOMERANG].tier;
    float spinSpeed = BOOMERANG_SPIN_SPEED + tier * 2;

    for (int i = 0; i < MAX_BOOMERANGS; i++) {
        BoomerangProj *b = &g_game.boomerangs[i];
        if (!b->active) continue;

        b->spinAngle += spinSpeed * dt;

        if (!b->returning) {
            // Outward travel
            b->pos.x += cosf(b->angle) * BOOMERANG_SPEED * dt;
            b->pos.y += sinf(b->angle) * BOOMERANG_SPEED * dt;
            b->outwardDist = Distance(b->startPos, b->pos);

            if (b->outwardDist >= b->maxDist) {
                b->returning = true;
            }
        } else {
            // Return to player
            Vector2 toPlayer = Normalize((Vector2){g_game.player.pos.x - b->pos.x, g_game.player.pos.y - b->pos.y});
            b->pos.x += toPlayer.x * BOOMERANG_SPEED * 1.2f * dt;
            b->pos.y += toPlayer.y * BOOMERANG_SPEED * 1.2f * dt;

            // Check if returned to player
            if (Distance(b->pos, g_game.player.pos) < PLAYER_SIZE + 10) {
                b->active = false;
                continue;
            }
        }

        // Hit enemies (pierce through) using spatial grid
        int hitList[8];
        int hitCount = 0;
        CheckEnemyCollisionAtPoint(b->pos, b->size, hitList, 8, &hitCount);
        for (int h = 0; h < hitCount; h++) {
            Enemy *e = &g_game.enemies[hitList[h]];
            if (!e->active) continue;
            DamageEnemy(e, b->damage);
        }
    }
}

static void DrawBoomerangs(void) {
    for (int i = 0; i < MAX_BOOMERANGS; i++) {
        BoomerangProj *b = &g_game.boomerangs[i];
        if (!b->active || !IsOnScreen(b->pos, 30)) continue;

        Vector2 screen = WorldToScreen(b->pos);

        // Motion blur trail
        for (int t = 0; t < 3; t++) {
            float trailAngle = b->spinAngle - t * 0.8f;
            float alpha = (3 - t) / 4.0f;
            Color c = COLOR_BOOMERANG;
            c.a = (unsigned char)(c.a * alpha * 0.5f);
            float sz = b->size * 0.8f;
            // Simple L-shape
            Vector2 p1 = {screen.x + cosf(trailAngle) * sz, screen.y + sinf(trailAngle) * sz};
            Vector2 p2 = {screen.x + cosf(trailAngle + 1.5f) * sz * 0.7f, screen.y + sinf(trailAngle + 1.5f) * sz * 0.7f};
            DrawLineEx(screen, p1, 4, c);
            DrawLineEx(screen, p2, 4, c);
        }

        // Main boomerang shape (L-shape with rotation)
        float sz = b->size;
        Vector2 p1 = {screen.x + cosf(b->spinAngle) * sz, screen.y + sinf(b->spinAngle) * sz};
        Vector2 p2 = {screen.x + cosf(b->spinAngle + 1.5f) * sz * 0.7f, screen.y + sinf(b->spinAngle + 1.5f) * sz * 0.7f};
        DrawLineEx(screen, p1, 5, COLOR_BOOMERANG);
        DrawLineEx(screen, p2, 5, COLOR_BOOMERANG);
        DrawCircleV(screen, 3, COLOR_BOOMERANG);
    }
}

// =============================================================================
// POISON WEAPON (Toxic Clouds)
// =============================================================================

static void SpawnPoisonCloud(void) {
    int tier = g_game.weapons[WEAPON_POISON].tier;
    int cloudCount = 1 + (tier > 2 ? 1 : 0) + (tier > 4 ? 1 : 0);

    for (int c = 0; c < cloudCount; c++) {
        // Find a spot near enemies
        Vector2 spawnPos = g_game.player.pos;
        int targetIdx = FindNearestEnemyGrid(g_game.player.pos, 300);
        if (targetIdx >= 0) {
            spawnPos = g_game.enemies[targetIdx].pos;
            spawnPos.x += RandomFloat(-30, 30);
            spawnPos.y += RandomFloat(-30, 30);
        }

        for (int i = 0; i < MAX_POISON_CLOUDS; i++) {
            PoisonCloud *p = &g_game.poisonClouds[i];
            if (!p->active) {
                p->pos = spawnPos;
                p->radius = (POISON_RADIUS + tier * 10) * GetAreaMultiplier();
                p->duration = POISON_DURATION + tier * 0.5f;
                p->timer = p->duration;
                p->tickTimer = POISON_TICK_RATE;
                p->damagePerTick = GetWeaponDamage(WEAPON_POISON);
                p->slowPercent = POISON_SLOW_PERCENT + tier * 5;
                p->active = true;
                p->pulsePhase = 0;
                break;
            }
        }
    }
}

static void UpdatePoisonClouds(float dt) {
    for (int i = 0; i < MAX_POISON_CLOUDS; i++) {
        PoisonCloud *p = &g_game.poisonClouds[i];
        if (!p->active) continue;

        p->timer -= dt;
        p->pulsePhase += dt * 2.0f;
        if (p->timer <= 0) { p->active = false; continue; }

        p->tickTimer -= dt;
        if (p->tickTimer <= 0) {
            p->tickTimer = POISON_TICK_RATE;

            // Damage and slow enemies in cloud using spatial grid
            int hitList[32];
            int hitCount = 0;
            CheckEnemyCollisionAtPoint(p->pos, p->radius, hitList, 32, &hitCount);

            for (int h = 0; h < hitCount; h++) {
                Enemy *e = &g_game.enemies[hitList[h]];
                if (!e->active) continue;
                DamageEnemy(e, p->damagePerTick);
                // Apply slow effect using timer-based system (doesn't compound)
                e->slowTimer = POISON_TICK_RATE + 0.1f;  // Lasts slightly longer than tick
                e->slowMultiplier = 1.0f - (p->slowPercent / 100.0f);
            }
        }

        // Spawn bubbling particles
        if (GetRandomValue(0, 100) < 15) {
            Vector2 particlePos = {
                p->pos.x + RandomFloat(-p->radius * 0.6f, p->radius * 0.6f),
                p->pos.y + RandomFloat(-p->radius * 0.6f, p->radius * 0.6f)
            };
            SpawnParticle(particlePos, (Vector2){0, -30}, COLOR_POISON, 3, 0.4f);
        }
    }
}

static void DrawPoisonClouds(void) {
    for (int i = 0; i < MAX_POISON_CLOUDS; i++) {
        PoisonCloud *p = &g_game.poisonClouds[i];
        if (!p->active || !IsOnScreen(p->pos, p->radius + 20)) continue;

        Vector2 screen = WorldToScreen(p->pos);
        float alpha = fminf(1.0f, p->timer / 1.0f);  // Fade out in last second
        float pulse = 1.0f + 0.1f * sinf(p->pulsePhase);
        float radius = p->radius * pulse;

        // Multiple overlapping circles for cloud effect
        Color c = COLOR_POISON;
        c.a = (unsigned char)(c.a * alpha * 0.6f);
        DrawCircleGradient((int)screen.x, (int)screen.y, radius, c, BLANK);

        c.a = (unsigned char)(c.a * 0.7f);
        DrawCircleGradient((int)(screen.x - radius * 0.3f), (int)(screen.y - radius * 0.2f), radius * 0.7f, c, BLANK);
        DrawCircleGradient((int)(screen.x + radius * 0.25f), (int)(screen.y + radius * 0.3f), radius * 0.6f, c, BLANK);

        // Border ring
        Color borderColor = COLOR_POISON;
        borderColor.a = (unsigned char)(100 * alpha);
        DrawCircleLines((int)screen.x, (int)screen.y, radius, borderColor);
    }
}

// =============================================================================
// CHAIN WEAPON (Chain Lightning)
// =============================================================================

static void TriggerChainLightning(void) {
    int tier = g_game.weapons[WEAPON_CHAIN].tier;

    int startTarget = FindNearestEnemyGrid(g_game.player.pos, CHAIN_RANGE);
    if (startTarget < 0) return;

    for (int i = 0; i < MAX_CHAINS; i++) {
        ChainLightning *c = &g_game.chains[i];
        if (!c->active) {
            c->hitCount = 0;
            c->hitEnemies[c->hitCount++] = startTarget;
            c->currentTarget = startTarget;
            c->remainingJumps = CHAIN_BASE_JUMPS + tier - 1;
            c->baseDamage = GetWeaponDamage(WEAPON_CHAIN);
            c->currentDamage = c->baseDamage;
            c->jumpRange = (CHAIN_JUMP_RANGE + tier * 10) * GetAreaMultiplier();
            c->timer = 0.4f;
            c->active = true;

            // Damage first target
            DamageEnemy(&g_game.enemies[startTarget], (int)c->currentDamage);
            SpawnParticleBurst(g_game.enemies[startTarget].pos, 4, COLOR_CHAIN, 60, 3);
            return;
        }
    }
}

static void UpdateChainLightning(float dt) {
    for (int i = 0; i < MAX_CHAINS; i++) {
        ChainLightning *c = &g_game.chains[i];
        if (!c->active) continue;

        c->timer -= dt;

        // Chain to next target periodically
        if (c->remainingJumps > 0 && c->timer < 0.35f - (0.35f - 0.05f * c->hitCount)) {
            // Find next target using spatial grid
            Enemy *current = &g_game.enemies[c->currentTarget];
            int nextTarget = -1;
            float nearestDistSq = c->jumpRange * c->jumpRange;

            // Use spatial grid to get nearby enemies
            int hitList[32];
            int hitCount = 0;
            CheckEnemyCollisionAtPoint(current->pos, c->jumpRange, hitList, 32, &hitCount);

            for (int h = 0; h < hitCount; h++) {
                int j = hitList[h];
                if (!g_game.enemies[j].active) continue;
                // Check if already hit
                bool alreadyHit = false;
                for (int k = 0; k < c->hitCount; k++) {
                    if (c->hitEnemies[k] == j) { alreadyHit = true; break; }
                }
                if (alreadyHit) continue;

                // Use squared distance to avoid sqrt
                float dx = current->pos.x - g_game.enemies[j].pos.x;
                float dy = current->pos.y - g_game.enemies[j].pos.y;
                float distSq = dx * dx + dy * dy;
                if (distSq < nearestDistSq) {
                    nearestDistSq = distSq;
                    nextTarget = j;
                }
            }

            if (nextTarget >= 0 && c->hitCount < 16) {
                c->hitEnemies[c->hitCount++] = nextTarget;
                c->currentTarget = nextTarget;
                c->remainingJumps--;
                c->currentDamage *= CHAIN_DECAY;

                DamageEnemy(&g_game.enemies[nextTarget], (int)c->currentDamage);
                SpawnParticleBurst(g_game.enemies[nextTarget].pos, 3, COLOR_CHAIN, 50, 2);
            }
        }

        if (c->timer <= 0) c->active = false;
    }
}

static void DrawChainLightning(void) {
    for (int i = 0; i < MAX_CHAINS; i++) {
        ChainLightning *c = &g_game.chains[i];
        if (!c->active) continue;

        float alpha = c->timer / 0.4f;

        // Draw arcs between all hit enemies
        for (int j = 0; j < c->hitCount - 1; j++) {
            if (!g_game.enemies[c->hitEnemies[j]].active) continue;
            if (!g_game.enemies[c->hitEnemies[j + 1]].active) continue;

            Vector2 from = WorldToScreen(g_game.enemies[c->hitEnemies[j]].pos);
            Vector2 to = WorldToScreen(g_game.enemies[c->hitEnemies[j + 1]].pos);

            Color arcColor = COLOR_CHAIN;
            arcColor.a = (unsigned char)(255 * alpha);

            // Jagged lightning effect
            Vector2 mid = {(from.x + to.x) / 2 + RandomFloat(-10, 10), (from.y + to.y) / 2 + RandomFloat(-10, 10)};
            DrawLineEx(from, mid, 3, arcColor);
            DrawLineEx(mid, to, 3, arcColor);

            // Glow at impact points
            DrawCircleGradient((int)to.x, (int)to.y, 12 * alpha, arcColor, BLANK);
        }

        // Draw arc from player to first target
        if (c->hitCount > 0 && g_game.enemies[c->hitEnemies[0]].active) {
            Vector2 playerScreen = WorldToScreen(g_game.player.pos);
            Vector2 firstTarget = WorldToScreen(g_game.enemies[c->hitEnemies[0]].pos);
            Color arcColor = COLOR_CHAIN;
            arcColor.a = (unsigned char)(200 * alpha);
            DrawLineEx(playerScreen, firstTarget, 2, arcColor);
        }
    }
}

static void UpdateWeapons(float dt) {
    // Update cooldowns and trigger weapons
    for (int i = 0; i < WEAPON_COUNT; i++) {
        WeaponSkill *w = &g_game.weapons[i];
        if (w->tier <= 0) continue;

        w->cooldownTimer -= dt;
        if (w->cooldownTimer <= 0) {
            w->cooldownTimer = GetWeaponCooldown((WeaponType)i);

            switch (i) {
                case WEAPON_MELEE: TriggerMelee(); break;
                case WEAPON_DISTANCE: FireBullet(); break;
                case WEAPON_MAGIC: if (!g_game.wave.active) TriggerWave(); break;
                case WEAPON_MYSTIC: TriggerLightning(); break;
                case WEAPON_SEEKER: FireSeeker(); break;
                case WEAPON_BOOMERANG: FireBoomerang(); break;
                case WEAPON_POISON: SpawnPoisonCloud(); break;
                case WEAPON_CHAIN: TriggerChainLightning(); break;
                default: break;
            }
        }
    }

    UpdateMelee(dt);
    UpdateProjectiles(dt);
    UpdateWave(dt);
    UpdateOrbit(dt);
    UpdateLightning(dt);
    UpdateSeekers(dt);
    UpdateBoomerangs(dt);
    UpdatePoisonClouds(dt);
    UpdateChainLightning(dt);
}


// =============================================================================
// DANGER ZONES
// =============================================================================

// Get XP multiplier for a position based on active danger zones
static float GetDangerZoneXPMultiplier(Vector2 pos) {
    for (int i = 0; i < MAX_DANGER_ZONES; i++) {
        DangerZone *dz = &g_game.dangerZones[i];
        if (!dz->active || dz->warningTimer > 0) continue;

        float dist = Distance(pos, dz->center);
        if (dist < dz->radius) {
            return dz->xpMultiplier;
        }
    }
    return 1.0f;  // No bonus
}

static void SpawnDangerZone(void) {
    // Find inactive slot
    int slot = -1;
    for (int i = 0; i < MAX_DANGER_ZONES; i++) {
        if (!g_game.dangerZones[i].active) {
            slot = i;
            break;
        }
    }
    if (slot < 0) return;  // All slots full

    DangerZone *dz = &g_game.dangerZones[slot];

    // Random position at least 200 units from player
    float angle = RandomFloat(0, PI * 2);
    float dist = DANGER_ZONE_MIN_PLAYER_DIST + RandomFloat(100, 300);
    Vector2 spawnPos = {
        Clampf(g_game.player.pos.x + cosf(angle) * dist,
               WORLD_PADDING + DANGER_ZONE_BASE_RADIUS,
               WORLD_WIDTH - WORLD_PADDING - DANGER_ZONE_BASE_RADIUS),
        Clampf(g_game.player.pos.y + sinf(angle) * dist,
               WORLD_PADDING + DANGER_ZONE_BASE_RADIUS,
               WORLD_HEIGHT - WORLD_PADDING - DANGER_ZONE_BASE_RADIUS)
    };

    // Random type
    DangerZoneType type = (DangerZoneType)(1 + GetRandomValue(0, 2));

    // XP multiplier based on danger level
    float xpMult = 1.5f;
    switch (type) {
        case DANGER_FIRE:     xpMult = 2.0f;  break;
        case DANGER_ELECTRIC: xpMult = 1.75f; break;
        case DANGER_SLOW:     xpMult = 1.5f;  break;
        default: break;
    }

    // Radius scales with wave
    float radius = DANGER_ZONE_BASE_RADIUS + g_game.spawner.wave * 5.0f;
    if (radius > 150.0f) radius = 150.0f;

    *dz = (DangerZone){
        .center = spawnPos,
        .radius = radius,
        .type = type,
        .timer = DANGER_ZONE_DURATION,
        .xpMultiplier = xpMult,
        .active = true,
        .warningTimer = DANGER_ZONE_WARNING,
        .damageTimer = 0
    };
}

static void UpdateDangerZones(float dt) {
    // Spawn timer
    g_game.dangerZoneSpawnTimer -= dt;
    if (g_game.dangerZoneSpawnTimer <= 0 && g_game.spawner.wave >= 2) {
        g_game.dangerZoneSpawnTimer = DANGER_ZONE_SPAWN_INTERVAL;
        SpawnDangerZone();
    }

    Player *player = &g_game.player;

    for (int i = 0; i < MAX_DANGER_ZONES; i++) {
        DangerZone *dz = &g_game.dangerZones[i];
        if (!dz->active) continue;

        // Warning phase
        if (dz->warningTimer > 0) {
            dz->warningTimer -= dt;
            continue;
        }

        // Active phase timer
        dz->timer -= dt;
        if (dz->timer <= 0) {
            dz->active = false;
            continue;
        }

        // Check if player is inside
        float dist = Distance(player->pos, dz->center);
        if (dist < dz->radius && player->invincibilityTimer <= 0) {
            switch (dz->type) {
                case DANGER_FIRE:
                    dz->damageTimer -= dt;
                    if (dz->damageTimer <= 0) {
                        dz->damageTimer = DANGER_ZONE_DAMAGE_TICK;
                        int fireDmg = DANGER_ZONE_FIRE_DAMAGE;
                        if (player->armor > 0) {
                            fireDmg = (int)(fireDmg * (1.0f - player->armor / 100.0f));
                            if (fireDmg < 1) fireDmg = 1;
                        }
                        player->hp -= fireDmg;
                        player->hurtFlash = 0.1f;
                        SpawnParticleBurst(player->pos, 3, COLOR_DANGER_FIRE, 40, 2);
                    }
                    break;

                case DANGER_SLOW:
                    // Slow player movement (handled in player update)
                    if ((int)(g_game.bgTime * 4) % 2 == 0) {
                        SpawnParticleBurst(player->pos, 1, COLOR_DANGER_SLOW, 20, 1);
                    }
                    break;

                case DANGER_ELECTRIC:
                    dz->damageTimer -= dt;
                    if (dz->damageTimer <= 0) {
                        dz->damageTimer = DANGER_ZONE_DAMAGE_TICK * 0.5f;
                        int elecDmg = DANGER_ZONE_ELECTRIC_DAMAGE;
                        if (player->armor > 0) {
                            elecDmg = (int)(elecDmg * (1.0f - player->armor / 100.0f));
                            if (elecDmg < 1) elecDmg = 1;
                        }
                        player->hp -= elecDmg;
                        player->hurtFlash = 0.05f;
                        SpawnParticleBurst(player->pos, 2, COLOR_DANGER_ELECTRIC, 50, 2);
                    }
                    break;

                default:
                    break;
            }
        }
    }
}

// Check if player is in a slow danger zone
static bool IsPlayerInSlowZone(void) {
    for (int i = 0; i < MAX_DANGER_ZONES; i++) {
        DangerZone *dz = &g_game.dangerZones[i];
        if (!dz->active || dz->warningTimer > 0 || dz->type != DANGER_SLOW) continue;

        float dist = Distance(g_game.player.pos, dz->center);
        if (dist < dz->radius) {
            return true;
        }
    }
    return false;
}
// =============================================================================

static void DrawDangerZones(void) {
    Vector2 camOffset = {
        g_game.camera.pos.x - g_screenWidth / 2.0f,
        g_game.camera.pos.y - g_screenHeight / 2.0f
    };

    for (int i = 0; i < MAX_DANGER_ZONES; i++) {
        DangerZone *dz = &g_game.dangerZones[i];
        if (!dz->active) continue;

        Vector2 screenPos = {dz->center.x - camOffset.x, dz->center.y - camOffset.y};

        // Cull if off-screen
        if (screenPos.x + dz->radius < 0 || screenPos.x - dz->radius > g_screenWidth ||
            screenPos.y + dz->radius < 0 || screenPos.y - dz->radius > g_screenHeight) {
            continue;
        }

        if (dz->warningTimer > 0) {
            // Warning phase - pulsing dashed circle
            float pulse = 0.5f + 0.5f * sinf(g_game.bgTime * 10.0f);
            Color warningColor = COLOR_DANGER_WARNING;
            warningColor.a = (unsigned char)(100 + 100 * pulse);

            DrawCircleLines((int)screenPos.x, (int)screenPos.y, dz->radius, warningColor);
            DrawCircleLines((int)screenPos.x, (int)screenPos.y, dz->radius * 0.95f, warningColor);

            const char *text = "DANGER";
            int textWidth = MeasureText(text, 16);
            DrawText(text, (int)screenPos.x - textWidth / 2, (int)screenPos.y - 8, 16, warningColor);
        } else {
            Color zoneColor;
            const char *bonusText;
            switch (dz->type) {
                case DANGER_FIRE:
                    zoneColor = COLOR_DANGER_FIRE;
                    bonusText = "2x XP";
                    break;
                case DANGER_SLOW:
                    zoneColor = COLOR_DANGER_SLOW;
                    bonusText = "1.5x XP";
                    break;
                case DANGER_ELECTRIC:
                    zoneColor = COLOR_DANGER_ELECTRIC;
                    bonusText = "1.75x XP";
                    break;
                default:
                    zoneColor = COLOR_DANGER_WARNING;
                    bonusText = "";
                    break;
            }

            float pulse = 0.8f + 0.2f * sinf(g_game.bgTime * 3.0f);
            zoneColor.a = (unsigned char)(zoneColor.a * pulse);

            DrawCircle((int)screenPos.x, (int)screenPos.y, dz->radius, zoneColor);

            Color borderColor = zoneColor;
            borderColor.a = 200;
            DrawCircleLines((int)screenPos.x, (int)screenPos.y, dz->radius, borderColor);
            DrawCircleLines((int)screenPos.x, (int)screenPos.y, dz->radius - 2, borderColor);

            int textWidth = MeasureText(bonusText, 20);
            DrawText(bonusText, (int)screenPos.x - textWidth / 2 + 1, (int)screenPos.y - 10 + 1, 20, BLACK);
            DrawText(bonusText, (int)screenPos.x - textWidth / 2, (int)screenPos.y - 10, 20, WHITE);

            float timerPercent = dz->timer / DANGER_ZONE_DURATION;
            char timerText[8];
            snprintf(timerText, sizeof(timerText), "%.0fs", dz->timer);
            int timerWidth = MeasureText(timerText, 12);
            DrawText(timerText, (int)screenPos.x - timerWidth / 2 + 1, (int)screenPos.y + 12 + 1, 12, BLACK);
            DrawText(timerText, (int)screenPos.x - timerWidth / 2, (int)screenPos.y + 12, 12,
                     (Color){255, 255, 255, (unsigned char)(150 + 105 * timerPercent)});
        }
    }
}

static void DrawDangerZonesOnMinimap(float scaleX, float scaleY) {
    for (int i = 0; i < MAX_DANGER_ZONES; i++) {
        DangerZone *dz = &g_game.dangerZones[i];
        if (!dz->active) continue;

        int mx = MINIMAP_X + (int)(dz->center.x * scaleX);
        int my = MINIMAP_Y + (int)(dz->center.y * scaleY);
        int mr = (int)(dz->radius * scaleX);
        if (mr < 2) mr = 2;

        Color zoneColor;
        if (dz->warningTimer > 0) {
            float pulse = 0.5f + 0.5f * sinf(g_game.bgTime * 10.0f);
            zoneColor = (Color){255, 200, 100, (unsigned char)(80 * pulse)};
        } else {
            switch (dz->type) {
                case DANGER_FIRE:     zoneColor = (Color){255, 100, 50, 100}; break;
                case DANGER_SLOW:     zoneColor = (Color){50, 150, 255, 100}; break;
                case DANGER_ELECTRIC: zoneColor = (Color){255, 255, 80, 100}; break;
                default:              zoneColor = (Color){255, 255, 255, 100}; break;
            }
        }

        DrawCircle(mx, my, mr, zoneColor);
    }
}
// SPAWN SYSTEM
// =============================================================================

static void UpdateSpawner(float dt) {
    SpawnSystem *s = &g_game.spawner;
    s->waveTimer += dt;

    if (s->waveTimer >= 30.0f) {
        s->wave++;
        s->waveTimer = 0;
        s->spawnInterval *= 0.9f;
        if (s->spawnInterval < 0.3f) s->spawnInterval = 0.3f;
        s->difficultyMultiplier += 0.15f;
        if (s->wave > g_game.highestWave) g_game.highestWave = s->wave;

        // Check for new enemy unlocks at this wave
        CheckWaveUnlocks(s->wave);

        // Wave celebration!
        TriggerWaveCelebration(s->wave);
    }

    s->spawnTimer -= dt;
    if (s->spawnTimer <= 0) {
        s->spawnTimer = s->spawnInterval;

        // Build list of available enemy types from unlocked pool
        EnemyType availableTypes[ENEMY_TYPE_COUNT];
        int availableCount = 0;
        for (int i = 0; i < ENEMY_TYPE_COUNT; i++) {
            if (g_enemyPoolUnlocked[i]) {
                availableTypes[availableCount++] = (EnemyType)i;
            }
        }

        // Weighted random selection based on enemy type
        int roll = GetRandomValue(0, 100);
        EnemyType spawnType = ENEMY_WALKER;

        // Boss has very low spawn chance (only if unlocked)
        if (g_enemyPoolUnlocked[ENEMY_BOSS] && roll < 2) {
            spawnType = ENEMY_BOSS;
            // Boss always gets a warning
            float spawnDist = 500.0f + RandomFloat(0, 200);
            float angle = RandomFloat(0, PI * 2);
            Vector2 spawnPos = {
                Clampf(g_game.player.pos.x + cosf(angle) * spawnDist, WORLD_PADDING, WORLD_WIDTH - WORLD_PADDING),
                Clampf(g_game.player.pos.y + sinf(angle) * spawnDist, WORLD_PADDING, WORLD_HEIGHT - WORLD_PADDING)
            };
            SpawnWarningIndicator(spawnPos, spawnType);
        }
        // Brute has low spawn chance
        else if (g_enemyPoolUnlocked[ENEMY_BRUTE] && roll < 8) {
            spawnType = ENEMY_BRUTE;
        }
        // Elite moderate chance
        else if (g_enemyPoolUnlocked[ENEMY_ELITE] && roll < 18) {
            spawnType = ENEMY_ELITE;
        }
        // Swarm spawns as group
        else if (g_enemyPoolUnlocked[ENEMY_SWARM] && roll < 28) {
            SpawnSwarm();
            return;  // Swarm handles its own spawning
        }
        // Tank
        else if (g_enemyPoolUnlocked[ENEMY_TANK] && roll < 40) {
            spawnType = ENEMY_TANK;
        }
        // Fast
        else if (g_enemyPoolUnlocked[ENEMY_FAST] && roll < 65) {
            spawnType = ENEMY_FAST;
        }
        // Default to walker
        else {
            spawnType = ENEMY_WALKER;
        }

        // Calculate spawn position for warning
        float spawnDist = 500.0f + RandomFloat(0, 200);
        float angle = RandomFloat(0, PI * 2);
        Vector2 spawnPos = {
            Clampf(g_game.player.pos.x + cosf(angle) * spawnDist, WORLD_PADDING, WORLD_WIDTH - WORLD_PADDING),
            Clampf(g_game.player.pos.y + sinf(angle) * spawnDist, WORLD_PADDING, WORLD_HEIGHT - WORLD_PADDING)
        };

        // Spawn warning indicator for dangerous enemies
        if (spawnType == ENEMY_TANK || spawnType == ENEMY_BRUTE || spawnType == ENEMY_ELITE) {
            SpawnWarningIndicator(spawnPos, spawnType);
        }

        SpawnEnemy(spawnType);

        // Extra spawns at higher waves
        if (s->wave >= 2 && GetRandomValue(0, 100) < 30) SpawnEnemy(ENEMY_WALKER);
        if (s->wave >= 4 && GetRandomValue(0, 100) < 20) SpawnEnemy(ENEMY_FAST);
        if (s->wave >= 8 && GetRandomValue(0, 100) < 15) SpawnSwarm();
    }
}

// =============================================================================
// PLAYER
// =============================================================================

static void UpdatePlayer(const LlzInputState *input, float dt) {
    Player *player = &g_game.player;

    // Toggle movement with select/space/enter OR left-click tap
    if (input->selectPressed || input->tap) player->isMoving = !player->isMoving;

    // Hold mouse button to force movement (auto-move while holding)
    if (input->mousePressed && input->dragActive) {
        float dragDist = sqrtf(
            (input->dragCurrent.x - input->dragStart.x) * (input->dragCurrent.x - input->dragStart.x) +
            (input->dragCurrent.y - input->dragStart.y) * (input->dragCurrent.y - input->dragStart.y)
        );
        // If dragging more than tap threshold, force movement on
        if (dragDist > 30.0f) {
            player->isMoving = true;
        }
    }

    // Mouse-based aiming: only update direction when mouse moves
    static Vector2 lastMousePos = {0};
    Vector2 mousePos = input->mousePos;
    float mouseDx = mousePos.x - lastMousePos.x;
    float mouseDy = mousePos.y - lastMousePos.y;
    float mouseMoveDist = sqrtf(mouseDx*mouseDx + mouseDy*mouseDy);

    // Only update angle if mouse actually moved (threshold to avoid micro-jitter)
    if (mouseMoveDist > 2.0f) {
        Vector2 playerScreen = WorldToScreen(player->pos);
        float dx = mousePos.x - playerScreen.x;
        float dy = mousePos.y - playerScreen.y;
        float distToMouse = sqrtf(dx*dx + dy*dy);

        // Only update angle if mouse is far enough from player (avoids jitter)
        if (distToMouse > 10.0f) {
            float targetAngle = atan2f(dy, dx);
            // Snap to target angle immediately when mouse moves
            player->angle = targetAngle;
        }
        lastMousePos = mousePos;
    }

    // Scroll wheel can still adjust angle (for fine-tuning or CarThing rotary)
    if (fabsf(input->scrollDelta) > 0.01f) player->angle += input->scrollDelta * 0.15f;

    float speed = player->speed * GetSpeedMultiplier();
    // Apply slow zone effect
    if (IsPlayerInSlowZone()) {
        speed *= DANGER_ZONE_SLOW_AMOUNT;
    }
    if (player->isMoving) {
        player->pos.x += cosf(player->angle) * speed * dt;
        player->pos.y += sinf(player->angle) * speed * dt;
        player->stationaryTime = 0;
    } else {
        player->stationaryTime += dt;
        // Health regen when stationary
        if (player->stationaryTime > 0.5f && player->healthRegen > 0 && player->hp < player->maxHp) {
            player->hp += (int)(player->healthRegen * dt);
            if (player->hp > player->maxHp) player->hp = player->maxHp;
        }
    }

    player->pos.x = Clampf(player->pos.x, WORLD_PADDING + PLAYER_SIZE / 2, WORLD_WIDTH - WORLD_PADDING - PLAYER_SIZE / 2);
    player->pos.y = Clampf(player->pos.y, WORLD_PADDING + PLAYER_SIZE / 2, WORLD_HEIGHT - WORLD_PADDING - PLAYER_SIZE / 2);

    if (player->invincibilityTimer > 0) player->invincibilityTimer -= dt;
    if (player->hurtFlash > 0) player->hurtFlash -= dt;
}

static void DrawPlayer(void) {
    Player *player = &g_game.player;
    Vector2 screen = WorldToScreen(player->pos);

    Color color = COLOR_PLAYER;
    if (player->hurtFlash > 0) color = COLOR_PLAYER_HURT;
    else if (player->invincibilityTimer > 0 && (int)(player->invincibilityTimer * 10) % 2 == 0) color.a = 100;
    if (HasShield()) { color.r = 255; color.g = 220; color.b = 80; }

    float hs = PLAYER_SIZE / 2;
    float c = cosf(player->angle + PI / 4), s = sinf(player->angle + PI / 4);
    Vector2 pts[4];
    float corners[4][2] = {{0, -hs}, {hs, 0}, {0, hs}, {-hs, 0}};
    for (int i = 0; i < 4; i++) {
        pts[i] = (Vector2){screen.x + corners[i][0] * c - corners[i][1] * s,
                           screen.y + corners[i][0] * s + corners[i][1] * c};
    }
    DrawTriangle(pts[0], pts[1], pts[2], color);
    DrawTriangle(pts[0], pts[2], pts[3], color);

    float arrowLen = PLAYER_SIZE * 0.8f;
    Vector2 arrowTip = {screen.x + cosf(player->angle) * arrowLen, screen.y + sinf(player->angle) * arrowLen};
    DrawLineEx(screen, arrowTip, 3, COLOR_PLAYER_ARROW);
    if (player->isMoving) DrawCircleV(arrowTip, 3, COLOR_PLAYER_ARROW);
}

// =============================================================================
// UPGRADE SYSTEM
// =============================================================================

static int GetNextTierCost(WeaponType weapon) {
    int currentTier = g_game.weapons[weapon].tier;
    if (currentTier >= MAX_SKILL_TIER) return 999;
    return SKILL_TIER_COSTS[currentTier];
}

// Get a random weapon that can be upgraded (tier > 0 and < MAX, and doesn't need branch selection)
static WeaponType GetRandomUpgradeableWeapon(void) {
    WeaponType candidates[WEAPON_COUNT];
    int count = 0;
    for (int i = 0; i < WEAPON_COUNT; i++) {
        WeaponSkill *w = &g_game.weapons[i];
        if (w->tier > 0 && w->tier < MAX_SKILL_TIER) {
            // Skip weapons at tier 2+ that need branch selection
            if (i < STARTING_WEAPON_COUNT && w->tier >= BRANCH_UNLOCK_TIER && w->branch == 0) {
                continue;  // Needs branch selection first
            }
            candidates[count++] = (WeaponType)i;
        }
    }
    if (count == 0) return WEAPON_COUNT;  // Invalid
    return candidates[GetRandomValue(0, count - 1)];
}

// Get a random weapon that can be unlocked (tier == 0)
static WeaponType GetRandomUnlockableWeapon(void) {
    WeaponType candidates[WEAPON_COUNT];
    int count = 0;
    for (int i = 0; i < WEAPON_COUNT; i++) {
        if (g_game.weapons[i].tier == 0) {
            candidates[count++] = (WeaponType)i;
        }
    }
    if (count == 0) return WEAPON_COUNT;  // Invalid
    return candidates[GetRandomValue(0, count - 1)];
}

// Get a random weapon that needs branch selection (tier >= 2, no branch chosen)
static WeaponType GetRandomBranchableWeapon(void) {
    WeaponType candidates[STARTING_WEAPON_COUNT];
    int count = 0;
    for (int i = 0; i < STARTING_WEAPON_COUNT; i++) {
        WeaponSkill *w = &g_game.weapons[i];
        if (w->tier >= BRANCH_UNLOCK_TIER && w->branch == 0) {
            candidates[count++] = (WeaponType)i;
        }
    }
    if (count == 0) return WEAPON_COUNT;  // Invalid
    return candidates[GetRandomValue(0, count - 1)];
}

// Get a random weapon with a branch that can be upgraded
static WeaponType GetRandomBranchUpgradeableWeapon(void) {
    WeaponType candidates[STARTING_WEAPON_COUNT];
    int count = 0;
    for (int i = 0; i < STARTING_WEAPON_COUNT; i++) {
        WeaponSkill *w = &g_game.weapons[i];
        if (w->branch > 0 && w->branchTier < MAX_BRANCH_TIER) {
            candidates[count++] = (WeaponType)i;
        }
    }
    if (count == 0) return WEAPON_COUNT;  // Invalid
    return candidates[GetRandomValue(0, count - 1)];
}

static void GenerateUpgradeChoices(void) {
    g_game.selectedUpgrade = NUM_UPGRADE_CHOICES / 2;  // Start in center
    g_game.carouselOffset = 0;
    g_game.targetOffset = 0;

    // Check if any weapon needs branch selection (priority)
    WeaponType branchableWeapon = GetRandomBranchableWeapon();
    bool hasBranchSelection = (branchableWeapon != WEAPON_COUNT);

    // Create pool of available upgrade indices (0-14 from UPGRADE_POOL)
    int pool[TOTAL_UPGRADE_TYPES];
    int poolSize = TOTAL_UPGRADE_TYPES;
    for (int i = 0; i < TOTAL_UPGRADE_TYPES; i++) pool[i] = i;

    // Shuffle pool
    for (int i = poolSize - 1; i > 0; i--) {
        int j = GetRandomValue(0, i);
        int temp = pool[i]; pool[i] = pool[j]; pool[j] = temp;
    }

    int chosen = 0;

    // If there's a weapon needing branch selection, offer all 3 branches first
    if (hasBranchSelection && chosen < NUM_UPGRADE_CHOICES) {
        for (int b = 1; b <= 3 && chosen < NUM_UPGRADE_CHOICES; b++) {
            UpgradeChoice *up = &g_game.upgrades[chosen];
            const BranchInfo *bi = GetBranchInfo(branchableWeapon, b);
            if (!bi) continue;

            up->type = UPGRADE_BRANCH_SELECT;
            up->weapon = branchableWeapon;
            up->branch = b;
            up->cost = 1;
            snprintf(up->name, sizeof(up->name), "%s: %s", WEAPON_NAMES[branchableWeapon], bi->name);
            snprintf(up->desc, sizeof(up->desc), "%s", bi->desc);
            up->value = 0;
            up->isOffensive = bi->isOffensive;
            up->available = g_game.player.upgradePoints >= up->cost;
            chosen++;
        }
    }

    // Check for branch tier upgrades
    WeaponType branchUpgradeable = GetRandomBranchUpgradeableWeapon();
    if (branchUpgradeable != WEAPON_COUNT && chosen < NUM_UPGRADE_CHOICES) {
        UpgradeChoice *up = &g_game.upgrades[chosen];
        WeaponSkill *skill = &g_game.weapons[branchUpgradeable];
        const BranchInfo *bi = GetBranchInfo(branchUpgradeable, skill->branch);

        up->type = UPGRADE_BRANCH_TIER;
        up->weapon = branchUpgradeable;
        up->branch = skill->branch;
        up->cost = 1 + skill->branchTier / 2;  // Cost increases with tier
        snprintf(up->name, sizeof(up->name), "%s+", bi->name);
        if (skill->branchTier < MAX_BRANCH_TIER && bi->tierDescs[skill->branchTier]) {
            snprintf(up->desc, sizeof(up->desc), "%s", bi->tierDescs[skill->branchTier]);
        } else {
            snprintf(up->desc, sizeof(up->desc), "Tier %d->%d", skill->branchTier, skill->branchTier + 1);
        }
        up->value = 0;
        up->isOffensive = bi->isOffensive;
        up->available = g_game.player.upgradePoints >= up->cost;
        chosen++;
    }

    // Fill remaining with regular upgrades from pool
    for (int i = 0; i < poolSize && chosen < NUM_UPGRADE_CHOICES; i++) {
        const UpgradeInfo *info = &UPGRADE_POOL[pool[i]];
        UpgradeChoice *up = &g_game.upgrades[chosen];

        // Skip weapon tier upgrade if no weapons can be upgraded
        if (info->type == UPGRADE_WEAPON_TIER) {
            WeaponType w = GetRandomUpgradeableWeapon();
            if (w == WEAPON_COUNT) continue;  // Skip this choice
            up->weapon = w;
            int cost = GetNextTierCost(w);
            up->cost = cost;
            snprintf(up->name, sizeof(up->name), "%s+", WEAPON_NAMES[w]);
            snprintf(up->desc, sizeof(up->desc), "Tier %d->%d", g_game.weapons[w].tier, g_game.weapons[w].tier + 1);
            up->value = 0;
            up->branch = 0;
        }
        // Skip weapon unlock if no weapons can be unlocked
        else if (info->type == UPGRADE_WEAPON_UNLOCK) {
            WeaponType w = GetRandomUnlockableWeapon();
            if (w == WEAPON_COUNT) continue;  // Skip this choice
            up->weapon = w;
            up->cost = 2;
            snprintf(up->name, sizeof(up->name), "Unlock %s", WEAPON_NAMES[w]);
            snprintf(up->desc, sizeof(up->desc), "%s", WEAPON_DESCS[w]);
            up->value = 0;
            up->branch = 0;
        }
        else {
            up->weapon = WEAPON_COUNT;
            up->branch = 0;
            up->cost = info->cost;
            snprintf(up->name, sizeof(up->name), "%s", info->name);
            snprintf(up->desc, sizeof(up->desc), info->descTemplate, info->baseValue);
            up->value = info->baseValue;
        }

        up->type = info->type;
        up->isOffensive = info->isOffensive;
        up->available = g_game.player.upgradePoints >= up->cost;
        chosen++;
    }

    // Skip option (always last)
    UpgradeChoice *skip = &g_game.upgrades[NUM_UPGRADE_CHOICES];
    skip->type = UPGRADE_SKIP;
    snprintf(skip->name, sizeof(skip->name), "Done");
    snprintf(skip->desc, sizeof(skip->desc), "Close shop & resume game");
    skip->cost = 0;
    skip->available = true;
    skip->isOffensive = false;
    skip->branch = 0;
}

// Apply upgrade effect only (for multi-purchase system)
// Does NOT change game state (caller handles that)
static void ApplyUpgradeEffect(int idx) {
    UpgradeChoice *up = &g_game.upgrades[idx];
    Player *player = &g_game.player;

    // Deduct from actual player points (session already tracked separately)
    player->upgradePoints -= up->cost;

    switch (up->type) {
        case UPGRADE_WEAPON_TIER:
            if (up->weapon < WEAPON_COUNT) {
                g_game.weapons[up->weapon].tier++;
            }
            break;
        case UPGRADE_WEAPON_UNLOCK:
            if (up->weapon < WEAPON_COUNT) {
                g_game.weapons[up->weapon].tier = 1;
                g_game.weapons[up->weapon].cooldownTimer = 0;
            }
            break;
        case UPGRADE_DAMAGE_ALL:
            player->damageMultiplier *= (1.0f + up->value / 100.0f);
            break;
        case UPGRADE_ATTACK_SPEED:
            player->attackSpeedMult *= (1.0f - up->value / 100.0f);
            if (player->attackSpeedMult < 0.2f) player->attackSpeedMult = 0.2f;
            break;
        case UPGRADE_CRIT_CHANCE:
            player->critChance += up->value;
            if (player->critChance > 75.0f) player->critChance = 75.0f;
            break;
        case UPGRADE_AREA_SIZE:
            player->areaMultiplier *= (1.0f + up->value / 100.0f);
            break;
        case UPGRADE_PROJECTILE_COUNT:
            player->bonusProjectiles += up->value;
            break;
        case UPGRADE_MAX_HP:
            player->maxHp += up->value;
            player->hp += up->value;
            break;
        case UPGRADE_HEALTH_REGEN:
            player->healthRegen += up->value;
            break;
        case UPGRADE_MOVE_SPEED:
            player->speed *= (1.0f + up->value / 100.0f);
            break;
        case UPGRADE_MAGNET_RANGE:
            player->magnetRange *= (1.0f + up->value / 100.0f);
            break;
        case UPGRADE_ARMOR:
            player->armor += up->value;
            if (player->armor > 80.0f) player->armor = 80.0f;
            break;
        case UPGRADE_LIFESTEAL:
            player->lifesteal += up->value;
            if (player->lifesteal > 50.0f) player->lifesteal = 50.0f;
            break;
        case UPGRADE_DODGE_CHANCE:
            player->dodgeChance += up->value;
            if (player->dodgeChance > 50.0f) player->dodgeChance = 50.0f;
            break;
        case UPGRADE_THORNS:
            player->thorns += up->value;
            if (player->thorns > 200.0f) player->thorns = 200.0f;
            break;
        case UPGRADE_BRANCH_SELECT:
            if (up->weapon < WEAPON_COUNT && up->branch > 0) {
                g_game.weapons[up->weapon].branch = up->branch;
                g_game.weapons[up->weapon].branchTier = 1;
                g_game.weapons[up->weapon].spinTimer = 0;
                g_game.weapons[up->weapon].spinning = false;
                g_game.weapons[up->weapon].pierceCount = 1;
                g_game.weapons[up->weapon].freezeAmount = 30;
                g_game.weapons[up->weapon].shieldHits = 1;
                g_game.weapons[up->weapon].chainJumps = 2;
            }
            break;
        case UPGRADE_BRANCH_TIER:
            if (up->weapon < WEAPON_COUNT) {
                g_game.weapons[up->weapon].branchTier++;
                int bt = g_game.weapons[up->weapon].branchTier;
                switch (up->weapon) {
                    case WEAPON_MELEE:
                        break;
                    case WEAPON_DISTANCE:
                        if (g_game.weapons[up->weapon].branch == DISTANCE_BRANCH_PIERCE) {
                            g_game.weapons[up->weapon].pierceCount = bt + 1;
                        }
                        break;
                    case WEAPON_MAGIC:
                        if (g_game.weapons[up->weapon].branch == MAGIC_BRANCH_FREEZE) {
                            g_game.weapons[up->weapon].freezeAmount = 30 + bt * 10;
                        }
                        break;
                    case WEAPON_RADIUS:
                        if (g_game.weapons[up->weapon].branch == RADIUS_BRANCH_SHIELD) {
                            g_game.weapons[up->weapon].shieldHits = bt + 1;
                        }
                        break;
                    case WEAPON_MYSTIC:
                        if (g_game.weapons[up->weapon].branch == MYSTIC_BRANCH_CHAIN) {
                            g_game.weapons[up->weapon].chainJumps = bt + 2;
                        }
                        break;
                    default: break;
                }
            }
            break;
        case UPGRADE_SKIP:
        default:
            break;
    }
}

static void ApplyUpgrade(int idx) {
    UpgradeChoice *up = &g_game.upgrades[idx];
    Player *player = &g_game.player;

    // Handle Done/Skip button - close shop and return to game
    if (up->type == UPGRADE_SKIP) {
        g_game.state = GAME_STATE_PLAYING;
        return;
    }

    // Don't allow purchasing unavailable upgrades
    if (!up->available) return;

    // Don't allow re-purchasing already purchased upgrades
    if (g_game.upgradesPurchasedThisSession[idx]) return;

    // Check if player can afford it
    if (g_game.sessionPointsRemaining < up->cost) return;

    // Deduct cost from both session and actual points
    g_game.sessionPointsRemaining -= up->cost;
    player->upgradePoints -= up->cost;

    // Apply the upgrade effect
    switch (up->type) {
        case UPGRADE_WEAPON_TIER:
            if (up->weapon < WEAPON_COUNT) {
                g_game.weapons[up->weapon].tier++;
            }
            break;
        case UPGRADE_WEAPON_UNLOCK:
            if (up->weapon < WEAPON_COUNT) {
                g_game.weapons[up->weapon].tier = 1;
                g_game.weapons[up->weapon].cooldownTimer = 0;
            }
            break;
        case UPGRADE_DAMAGE_ALL:
            player->damageMultiplier *= (1.0f + up->value / 100.0f);
            break;
        case UPGRADE_ATTACK_SPEED:
            player->attackSpeedMult *= (1.0f - up->value / 100.0f);
            if (player->attackSpeedMult < 0.2f) player->attackSpeedMult = 0.2f;
            break;
        case UPGRADE_CRIT_CHANCE:
            player->critChance += up->value;
            if (player->critChance > 75.0f) player->critChance = 75.0f;
            break;
        case UPGRADE_AREA_SIZE:
            player->areaMultiplier *= (1.0f + up->value / 100.0f);
            break;
        case UPGRADE_PROJECTILE_COUNT:
            player->bonusProjectiles += up->value;
            break;
        case UPGRADE_MAX_HP:
            player->maxHp += up->value;
            player->hp += up->value;
            break;
        case UPGRADE_HEALTH_REGEN:
            player->healthRegen += up->value;
            break;
        case UPGRADE_MOVE_SPEED:
            player->speed *= (1.0f + up->value / 100.0f);
            break;
        case UPGRADE_MAGNET_RANGE:
            player->magnetRange *= (1.0f + up->value / 100.0f);
            break;
        case UPGRADE_ARMOR:
            player->armor += up->value;
            if (player->armor > 80.0f) player->armor = 80.0f;
            break;
        case UPGRADE_LIFESTEAL:
            player->lifesteal += up->value;
            if (player->lifesteal > 50.0f) player->lifesteal = 50.0f;
            break;
        case UPGRADE_DODGE_CHANCE:
            player->dodgeChance += up->value;
            if (player->dodgeChance > 50.0f) player->dodgeChance = 50.0f;
            break;
        case UPGRADE_THORNS:
            player->thorns += up->value;
            if (player->thorns > 200.0f) player->thorns = 200.0f;
            break;
        case UPGRADE_BRANCH_SELECT:
            if (up->weapon < WEAPON_COUNT && up->branch > 0) {
                g_game.weapons[up->weapon].branch = up->branch;
                g_game.weapons[up->weapon].branchTier = 1;
                g_game.weapons[up->weapon].spinTimer = 0;
                g_game.weapons[up->weapon].spinning = false;
                g_game.weapons[up->weapon].pierceCount = 1;
                g_game.weapons[up->weapon].freezeAmount = 30;
                g_game.weapons[up->weapon].shieldHits = 1;
                g_game.weapons[up->weapon].chainJumps = 2;
            }
            break;
        case UPGRADE_BRANCH_TIER:
            if (up->weapon < WEAPON_COUNT) {
                g_game.weapons[up->weapon].branchTier++;
                int bt = g_game.weapons[up->weapon].branchTier;
                switch (up->weapon) {
                    case WEAPON_MELEE:
                        break;
                    case WEAPON_DISTANCE:
                        if (g_game.weapons[up->weapon].branch == DISTANCE_BRANCH_PIERCE) {
                            g_game.weapons[up->weapon].pierceCount = bt + 1;
                        }
                        break;
                    case WEAPON_MAGIC:
                        if (g_game.weapons[up->weapon].branch == MAGIC_BRANCH_FREEZE) {
                            g_game.weapons[up->weapon].freezeAmount = 30 + bt * 10;
                        }
                        break;
                    case WEAPON_RADIUS:
                        if (g_game.weapons[up->weapon].branch == RADIUS_BRANCH_SHIELD) {
                            g_game.weapons[up->weapon].shieldHits = bt + 1;
                        }
                        break;
                    case WEAPON_MYSTIC:
                        if (g_game.weapons[up->weapon].branch == MYSTIC_BRANCH_CHAIN) {
                            g_game.weapons[up->weapon].chainJumps = bt + 2;
                        }
                        break;
                    default: break;
                }
            }
            break;
        default:
            break;
    }

    // Mark this upgrade as purchased this session (prevents re-purchase)
    g_game.upgradesPurchasedThisSession[idx] = true;
}

// =============================================================================
// UI
// =============================================================================

static void DrawMinimap(void) {
    DrawRectangle(MINIMAP_X, MINIMAP_Y, MINIMAP_WIDTH, MINIMAP_HEIGHT, COLOR_MINIMAP_BG);
    DrawRectangleLinesEx((Rectangle){MINIMAP_X, MINIMAP_Y, MINIMAP_WIDTH, MINIMAP_HEIGHT}, 1, COLOR_MINIMAP_BORDER);

    float scaleX = (float)MINIMAP_WIDTH / WORLD_WIDTH;
    float scaleY = (float)MINIMAP_HEIGHT / WORLD_HEIGHT;

    // LOD: Limit draw calls when many entities exist
    // Use frame-based cycling to draw subsets
    int frameOffset = (int)(g_game.bgTime * 10) % 4;

    // Draw danger zones on minimap
    DrawDangerZonesOnMinimap(scaleX, scaleY);

    // Draw XP gems (limit to ~32 per frame by cycling through subsets)
    int xpDrawCount = 0;
    for (int i = frameOffset; i < MAX_XP_GEMS && xpDrawCount < 32; i += 4) {
        if (g_game.xpGems[i].active) {
            int mx = MINIMAP_X + (int)(g_game.xpGems[i].pos.x * scaleX);
            int my = MINIMAP_Y + (int)(g_game.xpGems[i].pos.y * scaleY);
            DrawPixel(mx, my, COLOR_MINIMAP_XP);
            xpDrawCount++;
        }
    }

    // Draw enemies (prioritize nearby enemies, limit total)
    // OPTIMIZED: Use squared distance to avoid sqrt() calls
    int enemyDrawCount = 0;
    Vector2 playerPos = g_game.player.pos;
    const float NEARBY_DIST_SQ = 400.0f * 400.0f;  // 160000.0f

    // First pass: draw nearby enemies (always visible)
    for (int i = 0; i < MAX_ENEMIES && enemyDrawCount < 48; i++) {
        Enemy *e = &g_game.enemies[i];
        if (!e->active) continue;

        // Squared distance avoids expensive sqrt()
        float dx = e->pos.x - playerPos.x;
        float dy = e->pos.y - playerPos.y;
        float distSq = dx * dx + dy * dy;
        if (distSq < NEARBY_DIST_SQ) {  // Always show nearby threats
            int mx = MINIMAP_X + (int)(e->pos.x * scaleX);
            int my = MINIMAP_Y + (int)(e->pos.y * scaleY);
            DrawPixel(mx, my, COLOR_MINIMAP_ENEMY);
            DrawPixel(mx + 1, my, COLOR_MINIMAP_ENEMY);
            DrawPixel(mx, my + 1, COLOR_MINIMAP_ENEMY);
            DrawPixel(mx + 1, my + 1, COLOR_MINIMAP_ENEMY);
            enemyDrawCount++;
        }
    }

    // Second pass: sample distant enemies (LOD)
    for (int i = frameOffset; i < MAX_ENEMIES && enemyDrawCount < 64; i += 2) {
        Enemy *e = &g_game.enemies[i];
        if (!e->active) continue;

        // Squared distance avoids expensive sqrt()
        float dx = e->pos.x - playerPos.x;
        float dy = e->pos.y - playerPos.y;
        float distSq = dx * dx + dy * dy;
        if (distSq >= NEARBY_DIST_SQ) {
            int mx = MINIMAP_X + (int)(e->pos.x * scaleX);
            int my = MINIMAP_Y + (int)(e->pos.y * scaleY);
            DrawPixel(mx, my, COLOR_MINIMAP_ENEMY);
            enemyDrawCount++;
        }
    }

    // Player always on top
    int px = MINIMAP_X + (int)(playerPos.x * scaleX);
    int py = MINIMAP_Y + (int)(playerPos.y * scaleY);
    DrawRectangle(px - 2, py - 2, 4, 4, COLOR_MINIMAP_PLAYER);

    // Viewport indicator
    float viewX = g_game.camera.pos.x - g_screenWidth / 2.0f;
    float viewY = g_game.camera.pos.y - g_screenHeight / 2.0f;
    DrawRectangleLinesEx((Rectangle){
        MINIMAP_X + (int)(viewX * scaleX), MINIMAP_Y + (int)(viewY * scaleY),
        (int)(g_screenWidth * scaleX), (int)(g_screenHeight * scaleY)
    }, 1, (Color){255, 255, 255, 100});
}

static void DrawInventory(void) {
    int startX = 10, y = g_screenHeight - 35;
    DrawTextEx(g_font, "Potions:", (Vector2){startX, y - 15}, 12, 1, COLOR_TEXT_DIM);

    // Count active potions for display
    int activePotionCount = 0;
    for (int i = 0; i < MAX_INVENTORY_POTIONS; i++) {
        if (g_game.inventory[i].active) activePotionCount++;
    }

    for (int i = 0; i < MAX_INVENTORY_POTIONS; i++) {
        int x = startX + i * 28;
        bool isSelected = (i == g_game.selectedPotion);
        bool hasPotion = g_game.inventory[i].active;

        // Draw slot background
        Color bgColor = hasPotion ? (Color){30, 30, 45, 230} : COLOR_UI_BG;
        DrawRectangle(x, y, 24, 24, bgColor);

        // Highlight selected slot
        Color borderColor = isSelected ? COLOR_UPGRADE_SEL : COLOR_TEXT_DIM;
        int borderWidth = isSelected ? 2 : 1;
        DrawRectangleLinesEx((Rectangle){x, y, 24, 24}, borderWidth, borderColor);

        if (hasPotion) {
            PotionType type = g_game.inventory[i].type;
            Color c = GetPotionColor(type);

            // Draw potion circle with glow
            if (isSelected) {
                DrawCircleV((Vector2){x + 12, y + 12}, 10, (Color){c.r, c.g, c.b, 60});
            }
            DrawCircleV((Vector2){x + 12, y + 12}, 7, c);

            // Draw symbol
            const char *symbol = GetPotionSymbol(type);
            DrawTextEx(g_font, symbol, (Vector2){x + 9, y + 8}, 10, 0, WHITE);

            // Show slot number
            char slotNum[2] = {'1' + i, '\0'};
            DrawTextEx(g_font, slotNum, (Vector2){x + 2, y + 2}, 8, 0, COLOR_TEXT_DIM);
        }
    }

    // Show selected potion info tooltip
    if (activePotionCount > 0 && g_game.inventory[g_game.selectedPotion].active) {
        PotionType type = g_game.inventory[g_game.selectedPotion].type;
        const char *name = GetPotionName(type);
        const char *desc = GetPotionDesc(type);
        Color c = GetPotionColor(type);

        int tooltipX = startX;
        int tooltipY = y - 28;
        DrawTextEx(g_font, name, (Vector2){tooltipX, tooltipY}, 12, 1, c);
        DrawTextEx(g_font, desc, (Vector2){tooltipX, tooltipY + 12}, 10, 1, COLOR_TEXT_DIM);
    }

    // Show controls hint
    DrawTextEx(g_font, "UP:Select DOWN:Use", (Vector2){startX + 145, y + 8}, 9, 1, COLOR_TEXT_DIM);
}

static void DrawActiveBuffs(void) {
    int x = 10, y = g_screenHeight - 90;

    // Check if any buffs active
    bool hasBuffs = false;
    for (int i = 0; i < POTION_COUNT; i++) {
        if (g_game.buffs[i].active) { hasBuffs = true; break; }
    }
    if (!hasBuffs) return;

    DrawTextEx(g_font, "ACTIVE:", (Vector2){x, y - 12}, 10, 1, COLOR_TEXT_DIM);

    for (int i = 0; i < POTION_COUNT; i++) {
        if (g_game.buffs[i].active) {
            Color c = GetPotionColor((PotionType)i);
            float ratio = g_game.buffs[i].timer / g_game.buffs[i].duration;
            int seconds = (int)g_game.buffs[i].timer;

            // Draw buff icon
            DrawCircleV((Vector2){x + 8, y + 6}, 6, c);
            const char *symbol = GetPotionSymbol((PotionType)i);
            DrawTextEx(g_font, symbol, (Vector2){x + 5, y + 2}, 8, 0, WHITE);

            // Draw timer bar
            DrawRectangle(x + 18, y, (int)(45 * ratio), 12, c);
            DrawRectangleLinesEx((Rectangle){x + 18, y, 45, 12}, 1, WHITE);

            // Draw time remaining
            char timeStr[8];
            snprintf(timeStr, sizeof(timeStr), "%ds", seconds);
            DrawTextEx(g_font, timeStr, (Vector2){x + 66, y + 1}, 10, 1, COLOR_TEXT);

            x += 90;
        }
    }
}

// Draw active weapon synergies indicator
static void DrawSynergies(void) {
    int synCount = CountActiveSynergies();
    if (synCount == 0) return;

    // Draw synergy indicator in top-right area (below minimap)
    int x = MINIMAP_X;
    int y = MINIMAP_Y + MINIMAP_HEIGHT + 10;

    // Synergy icon (interlocked rings effect)
    Color synColor = (Color){255, 200, 100, 200};
    float pulse = 0.8f + 0.2f * sinf(g_game.bgTime * 3.0f);

    DrawCircleLines(x + 8, y + 8, 6, synColor);
    DrawCircleLines(x + 14, y + 8, 6, synColor);

    char synText[32];
    snprintf(synText, sizeof(synText), "x%d", synCount);
    DrawTextEx(g_font, synText, (Vector2){x + 22, y + 2}, 12 * pulse, 1, synColor);

    // Show first active synergy name on hover area
    for (int i = 0; i < (int)NUM_SYNERGIES; i++) {
        if (IsSynergyActive(&WEAPON_SYNERGIES[i])) {
            Color nameColor = {255, 220, 150, 180};
            DrawTextEx(g_font, WEAPON_SYNERGIES[i].name, (Vector2){x, y + 16}, 9, 1, nameColor);
            break;
        }
    }
}

// Draw red danger glow on screen edges for nearby off-screen enemies
// OPTIMIZED: Use gradient rectangles instead of 40+ individual draw calls per edge
static void DrawDangerGlow(void) {
    const int GLOW_WIDTH_H = 40;  // Horizontal edges (left/right)
    const int GLOW_WIDTH_V = 30;  // Vertical edges (top/bottom)

    // Left edge - single gradient rectangle
    if (g_dangerGlow[0] > 0.01f) {
        float intensity = g_dangerGlow[0];
        float pulse = 0.7f + 0.3f * sinf(g_game.bgTime * 8.0f);
        intensity *= pulse;

        Color edgeColor = {255, 50, 50, (unsigned char)(100 * intensity)};
        Color transparent = {255, 50, 50, 0};
        DrawRectangleGradientH(0, 0, GLOW_WIDTH_H, g_screenHeight, edgeColor, transparent);
    }

    // Right edge - single gradient rectangle
    if (g_dangerGlow[1] > 0.01f) {
        float intensity = g_dangerGlow[1];
        float pulse = 0.7f + 0.3f * sinf(g_game.bgTime * 8.0f + 1.0f);
        intensity *= pulse;

        Color edgeColor = {255, 50, 50, (unsigned char)(100 * intensity)};
        Color transparent = {255, 50, 50, 0};
        DrawRectangleGradientH(g_screenWidth - GLOW_WIDTH_H, 0, GLOW_WIDTH_H, g_screenHeight, transparent, edgeColor);
    }

    // Top edge - single gradient rectangle
    if (g_dangerGlow[2] > 0.01f) {
        float intensity = g_dangerGlow[2];
        float pulse = 0.7f + 0.3f * sinf(g_game.bgTime * 8.0f + 2.0f);
        intensity *= pulse;

        Color edgeColor = {255, 50, 50, (unsigned char)(100 * intensity)};
        Color transparent = {255, 50, 50, 0};
        DrawRectangleGradientV(0, 0, g_screenWidth, GLOW_WIDTH_V, edgeColor, transparent);
    }

    // Bottom edge - single gradient rectangle
    if (g_dangerGlow[3] > 0.01f) {
        float intensity = g_dangerGlow[3];
        float pulse = 0.7f + 0.3f * sinf(g_game.bgTime * 8.0f + 3.0f);
        intensity *= pulse;

        Color edgeColor = {255, 50, 50, (unsigned char)(100 * intensity)};
        Color transparent = {255, 50, 50, 0};
        DrawRectangleGradientV(0, g_screenHeight - GLOW_WIDTH_V, g_screenWidth, GLOW_WIDTH_V, transparent, edgeColor);
    }
}

static void DrawHUD(void) {
    Player *player = &g_game.player;

    // Track HP changes for damage flash
    float hpRatio = (float)player->hp / player->maxHp;
    if (player->hp < g_hpPrevValue) {
        g_hpFlash = 1.0f;  // Trigger flash on damage
    }
    g_hpPrevValue = (float)player->hp;

    // HP bar dimensions with shake on damage
    int hpBarX = 10 + (int)(g_hpFlash * sinf(g_game.bgTime * 40) * 3);
    int hpBarY = 10;
    int hpBarW = 200;
    int hpBarH = 16;

    // Low HP pulsing glow
    bool lowHP = hpRatio < LOW_HP_THRESHOLD;
    if (lowHP) {
        float pulse = 0.5f + 0.5f * sinf(g_lowHpPulse * 6.0f);  // Fast pulse
        Color dangerGlow = {255, 50, 50, (unsigned char)(100 * pulse)};
        DrawCircleGradient(hpBarX + hpBarW / 2, hpBarY + hpBarH / 2, 120, dangerGlow, BLANK);
    }

    // HP bar background
    DrawRectangle(hpBarX, hpBarY, hpBarW, hpBarH, COLOR_HP_BG);

    // HP bar fill with color based on health
    Color hpColor = COLOR_HP_BAR;
    if (lowHP) {
        // Pulse between red and dark red when low
        float pulse = 0.5f + 0.5f * sinf(g_lowHpPulse * 8.0f);
        hpColor.r = (unsigned char)(150 + 105 * pulse);
        hpColor.g = (unsigned char)(20 + 30 * pulse);
        hpColor.b = (unsigned char)(20 + 30 * pulse);
    }
    int hpFillW = (int)(hpBarW * hpRatio);
    DrawRectangle(hpBarX, hpBarY, hpFillW, hpBarH, hpColor);

    // Damage flash overlay (white flash on HP bar)
    if (g_hpFlash > 0) {
        Color flashColor = {255, 255, 255, (unsigned char)(180 * g_hpFlash)};
        DrawRectangle(hpBarX, hpBarY, hpFillW, hpBarH, flashColor);
    }

    // HP bar border
    Color borderColor = lowHP ? (Color){255, 100, 100, 255} : COLOR_TEXT;
    DrawRectangleLines(hpBarX, hpBarY, hpBarW, hpBarH, borderColor);

    // HP text indicator when damaged
    if (g_hpFlash > 0.3f) {
        char hpText[16];
        snprintf(hpText, sizeof(hpText), "%d/%d", player->hp, player->maxHp);
        Font hpFont = LlzFontGet(LLZ_FONT_UI, 12);
        int htw = (int)MeasureTextEx(hpFont, hpText, 12, 1).x;
        Color hpTextColor = {255, 255, 255, (unsigned char)(255 * (g_hpFlash - 0.3f) / 0.7f)};
        DrawTextEx(hpFont, hpText, (Vector2){hpBarX + hpBarW / 2 - htw / 2, hpBarY + 2}, 12, 1, hpTextColor);
    }

    // XP bar with pulse effect
    float pulse = g_game.xpBarPulse;
    float barWidth = 150 + 4 * pulse;
    float barHeight = 8 + 2 * pulse;
    int barX = 10 - (int)(2 * pulse);
    int barY = 30 - (int)(1 * pulse);

    DrawRectangle(barX, barY, (int)barWidth, (int)barHeight, COLOR_XP_BG);
    float xpRatio = player->level < MAX_LEVEL ? (float)player->xp / player->xpToNextLevel : 1.0f;
    DrawRectangle(barX, barY, (int)(barWidth * xpRatio), (int)barHeight, COLOR_XP_BAR);

    // Glow when pulsing
    if (pulse > 0) {
        Color glowColor = COLOR_XP_BAR;
        glowColor.a = (unsigned char)(100 * pulse);
        DrawRectangle(barX, barY, (int)(barWidth * xpRatio), (int)barHeight, glowColor);
    }

    DrawRectangleLines(barX, barY, (int)barWidth, (int)barHeight, COLOR_TEXT_DIM);

    // Level-up anticipation glow when XP bar is > 80% full
    if (xpRatio > 0.8f && player->level < MAX_LEVEL) {
        float intensity = (xpRatio - 0.8f) / 0.2f;
        float glowPulse = 0.5f + 0.5f * sinf(g_game.bgTime * 4.0f);
        Color glowColor = COLOR_XP_BAR;
        glowColor.a = (unsigned char)(40 * intensity * glowPulse);
        DrawCircleGradient(barX + (int)(barWidth * xpRatio) / 2, barY + 4, 50 * intensity, glowColor, BLANK);
    }

    char buf[64];
    snprintf(buf, sizeof(buf), "LV %d  Pts: %d", player->level, player->upgradePoints);
    DrawTextEx(g_font, buf, (Vector2){165, 26}, 14, 1, COLOR_TEXT);

    int mins = (int)g_game.gameTime / 60, secs = (int)g_game.gameTime % 60;
    snprintf(buf, sizeof(buf), "%d:%02d", mins, secs);
    int tw = (int)MeasureTextEx(g_font, buf, 18, 1).x;
    DrawTextEx(g_font, buf, (Vector2){g_screenWidth / 2 - tw / 2, 10}, 18, 1, COLOR_TEXT);

    snprintf(buf, sizeof(buf), "Kills: %d  Wave %d", g_game.killCount, g_game.spawner.wave + 1);
    tw = (int)MeasureTextEx(g_font, buf, 14, 1).x;
    DrawTextEx(g_font, buf, (Vector2){g_screenWidth / 2 - tw / 2, 30}, 14, 1, COLOR_TEXT_DIM);

    // Kill streak display (bottom left, above inventory)
    if (g_killStreak >= 3) {
        // Calculate XP multiplier from streak
        float xpMult = 1.0f + (g_killStreak / 10.0f);
        if (xpMult > 3.0f) xpMult = 3.0f;  // Cap at 3x

        // Pulsing effect when streak is active
        float pulse = 0.8f + 0.2f * sinf(g_game.bgTime * 6.0f);
        int streakY = g_screenHeight - 100;

        // Streak count with fire colors
        Color streakColor;
        if (g_killStreak >= 50) streakColor = (Color){255, 50, 50, 255};      // Red hot
        else if (g_killStreak >= 25) streakColor = (Color){255, 150, 50, 255}; // Orange
        else if (g_killStreak >= 10) streakColor = (Color){255, 200, 50, 255}; // Yellow
        else streakColor = (Color){200, 200, 255, 255};                         // White-blue

        snprintf(buf, sizeof(buf), "%dx STREAK", g_killStreak);
        Font streakFont = LlzFontGet(LLZ_FONT_UI, (int)(18 * pulse));
        int sw = (int)MeasureTextEx(streakFont, buf, 18 * pulse, 1).x;
        DrawTextEx(streakFont, buf, (Vector2){10, streakY}, 18 * pulse, 1, streakColor);

        // XP multiplier indicator
        snprintf(buf, sizeof(buf), "XP x%.1f", xpMult);
        Color multColor = {100, 255, 100, 200};
        DrawTextEx(g_font, buf, (Vector2){10, streakY + 20}, 12, 1, multColor);
    }

    // Kill streak milestone announcement (center screen)
    if (g_killStreakDisplay > 0 && g_killStreakMilestone >= 0 && g_killStreakMilestone < NUM_KILL_MILESTONES) {
        float progress = g_killStreakDisplay / KILL_STREAK_DISPLAY_TIME;
        float scale = EaseOutBack(fminf(1.0f, progress * 2.0f));
        float alpha = progress;

        const char *milestone = KILL_MILESTONE_NAMES[g_killStreakMilestone];
        Font milestoneFont = LlzFontGet(LLZ_FONT_UI, (int)(28 * scale));
        int mw = (int)MeasureTextEx(milestoneFont, milestone, 28 * scale, 1).x;

        Color milestoneColor = {255, 215, 0, (unsigned char)(255 * alpha)};
        DrawTextEx(milestoneFont, milestone,
                   (Vector2){g_screenWidth / 2 - mw / 2, g_screenHeight / 2 - 80},
                   28 * scale, 1, milestoneColor);
    }

    // Graze combo display (bottom right)
    if (g_game.grazeCombo >= 2) {
        float pulse = 0.8f + 0.2f * sinf(g_game.bgTime * 8.0f);
        Color grazeColor = {255, 220, 100, (unsigned char)(200 * pulse)};

        snprintf(buf, sizeof(buf), "GRAZE x%d", g_game.grazeCombo);
        int gw = (int)MeasureTextEx(g_font, buf, 14, 1).x;
        DrawTextEx(g_font, buf, (Vector2){g_screenWidth - gw - 10, g_screenHeight - 100}, 14, 1, grazeColor);
    }

    // Graze flash effect
    if (g_game.grazeFlash > 0) {
        Color flashColor = {255, 220, 100, (unsigned char)(50 * g_game.grazeFlash)};
        DrawRectangle(0, 0, g_screenWidth, g_screenHeight, flashColor);
    }

    DrawMinimap();
    DrawMilestoneProgressHUD();  // Milestone progress bar below minimap
    DrawInventory();
    DrawActiveBuffs();
    DrawSynergies();
}

static void DrawLevelUpScreen(void) {
    DrawRectangle(0, 0, g_screenWidth, g_screenHeight, (Color){0, 0, 0, 200});

    // Title - show session points remaining
    char title[64];
    snprintf(title, sizeof(title), "LEVEL UP!  Points: %d", g_game.sessionPointsRemaining);
    int tw = (int)MeasureTextEx(g_font, title, 32, 1).x;
    DrawTextEx(g_font, title, (Vector2){g_screenWidth / 2 - tw / 2, 20}, 32, 1, COLOR_XP_BAR);

    // Smooth carousel animation
    g_game.carouselOffset = Lerpf(g_game.carouselOffset, g_game.targetOffset, 0.15f);

    // Calculate carousel positions
    int totalChoices = NUM_UPGRADE_CHOICES + 1;
    float cardW = CAROUSEL_CARD_WIDTH;
    float cardH = CAROUSEL_CARD_HEIGHT;
    float spacing = CAROUSEL_SPACING;
    float centerX = g_screenWidth / 2.0f;
    float centerY = CAROUSEL_Y + cardH / 2.0f;

    // Draw cards from left to right
    for (int i = 0; i < totalChoices; i++) {
        UpgradeChoice *up = &g_game.upgrades[i];

        // Position relative to selected card
        float relPos = i - g_game.selectedUpgrade - g_game.carouselOffset;
        float x = centerX + relPos * (cardW + spacing) - cardW / 2.0f;

        // Skip if too far off screen
        if (x < -cardW - 50 || x > g_screenWidth + 50) continue;

        // Scale and alpha based on distance from center
        float dist = fabsf(relPos);
        float scale = 1.0f - dist * 0.15f;
        if (scale < 0.6f) scale = 0.6f;
        float alpha = 1.0f - dist * 0.3f;
        if (alpha < 0.3f) alpha = 0.3f;

        float scaledW = cardW * scale;
        float scaledH = cardH * scale;
        float drawX = x + (cardW - scaledW) / 2.0f;
        float drawY = centerY - scaledH / 2.0f + dist * 15.0f;  // Offset Y for depth effect

        // Check if card was purchased this session
        bool isPurchased = g_game.upgradesPurchasedThisSession[i];
        bool canAfford = g_game.sessionPointsRemaining >= up->cost;

        // Card colors - gray out if purchased
        Color bgColor;
        if (isPurchased) {
            // Purchased - grayed out with green tint
            bgColor = (Color){40, 60, 40, (unsigned char)(180 * alpha)};
        } else if (up->type == UPGRADE_SKIP) {
            bgColor = (Color){60, 60, 80, (unsigned char)(220 * alpha)};
        } else if (up->isOffensive) {
            bgColor = canAfford ?
                (Color){80, 40, 40, (unsigned char)(240 * alpha)} :
                (Color){50, 30, 30, (unsigned char)(200 * alpha)};
        } else {
            bgColor = canAfford ?
                (Color){40, 60, 80, (unsigned char)(240 * alpha)} :
                (Color){30, 40, 50, (unsigned char)(200 * alpha)};
        }

        // Draw card background with rounded corners effect (just rectangles for simplicity)
        DrawRectangle((int)drawX, (int)drawY, (int)scaledW, (int)scaledH, bgColor);

        // Border (thicker for selected, green for purchased)
        bool isSelected = (i == g_game.selectedUpgrade && fabsf(g_game.carouselOffset) < 0.1f && g_game.levelUpMode == 0);
        Color borderColor;
        if (isPurchased) {
            borderColor = (Color){80, 200, 80, (unsigned char)(200 * alpha)};  // Green for purchased
        } else if (isSelected) {
            borderColor = COLOR_UPGRADE_SEL;
        } else {
            borderColor = (Color){100, 100, 120, (unsigned char)(200 * alpha)};
        }
        int borderThick = isSelected ? 4 : 2;
        DrawRectangleLinesEx((Rectangle){drawX, drawY, scaledW, scaledH}, borderThick, borderColor);

        // Card content - dim if purchased
        float fontSize = 18.0f * scale;
        float descSize = 13.0f * scale;
        float costSize = 14.0f * scale;
        float textAlpha = isPurchased ? alpha * 0.5f : alpha;
        Color textColor = {(unsigned char)(255 * textAlpha), (unsigned char)(255 * textAlpha), (unsigned char)(255 * textAlpha), 255};
        Color dimColor = {(unsigned char)(180 * textAlpha), (unsigned char)(180 * textAlpha), (unsigned char)(200 * textAlpha), 255};

        // Type indicator (sword for offensive, shield for defensive)
        const char *typeIcon = up->isOffensive ? "[ATK]" : "[DEF]";
        if (up->type == UPGRADE_SKIP) typeIcon = "[---]";
        Color typeColor = up->isOffensive ? COLOR_POTION_DAMAGE : COLOR_POTION_SPEED;
        typeColor.a = (unsigned char)(typeColor.a * textAlpha);
        DrawTextEx(g_font, typeIcon, (Vector2){drawX + 8, drawY + 8}, 12 * scale, 1, typeColor);

        // Name
        int nw = (int)MeasureTextEx(g_font, up->name, fontSize, 1).x;
        DrawTextEx(g_font, up->name, (Vector2){drawX + scaledW / 2 - nw / 2, drawY + 30 * scale}, fontSize, 1, textColor);

        // Description (centered, wrapped if needed)
        int dw = (int)MeasureTextEx(g_font, up->desc, descSize, 1).x;
        float descX = drawX + scaledW / 2 - dw / 2;
        if (descX < drawX + 5) descX = drawX + 5;
        DrawTextEx(g_font, up->desc, (Vector2){descX, drawY + 60 * scale}, descSize, 1, dimColor);

        // Cost or status
        if (isPurchased) {
            // Show PURCHASED label
            const char *purchasedStr = "PURCHASED";
            int pw = (int)MeasureTextEx(g_font, purchasedStr, costSize, 1).x;
            DrawTextEx(g_font, purchasedStr, (Vector2){drawX + scaledW / 2 - pw / 2, drawY + scaledH - 35 * scale}, costSize, 1, (Color){80, 200, 80, (unsigned char)(255 * alpha)});
        } else if (up->cost > 0) {
            char costStr[32];
            snprintf(costStr, sizeof(costStr), "Cost: %d point%s", up->cost, up->cost > 1 ? "s" : "");
            int cw = (int)MeasureTextEx(g_font, costStr, costSize, 1).x;
            Color costColor = canAfford ?
                (Color){80, 200, 255, (unsigned char)(255 * alpha)} :
                (Color){200, 80, 80, (unsigned char)(255 * alpha)};
            DrawTextEx(g_font, costStr, (Vector2){drawX + scaledW / 2 - cw / 2, drawY + scaledH - 35 * scale}, costSize, 1, costColor);
        }

        // Availability indicator
        if (!canAfford && !isPurchased && up->type != UPGRADE_SKIP) {
            DrawTextEx(g_font, "CAN'T AFFORD", (Vector2){drawX + scaledW / 2 - 40, drawY + scaledH - 20 * scale}, 12 * scale, 1, COLOR_WALKER);
        }
    }

    // Navigation arrows
    DrawTriangle(
        (Vector2){30, centerY - 15}, (Vector2){50, centerY}, (Vector2){30, centerY + 15},
        g_game.selectedUpgrade > 0 ? COLOR_TEXT : COLOR_TEXT_DIM);
    DrawTriangle(
        (Vector2){g_screenWidth - 30, centerY - 15}, (Vector2){g_screenWidth - 50, centerY}, (Vector2){g_screenWidth - 30, centerY + 15},
        g_game.selectedUpgrade < totalChoices - 1 ? COLOR_TEXT : COLOR_TEXT_DIM);

    // Instructions
    // Confirm button
    int confirmBtnW = 200;
    int confirmBtnH = 40;
    int confirmBtnX = g_screenWidth / 2 - confirmBtnW / 2;
    int confirmBtnY = CAROUSEL_Y + CAROUSEL_CARD_HEIGHT + 30;

    bool confirmSelected = (g_game.levelUpMode == 1);
    Color confirmBgColor = confirmSelected ? (Color){60, 150, 60, 240} : (Color){40, 80, 40, 200};
    Color confirmBorderColor = confirmSelected ? COLOR_UPGRADE_SEL : (Color){80, 120, 80, 200};

    DrawRectangle(confirmBtnX, confirmBtnY, confirmBtnW, confirmBtnH, confirmBgColor);
    DrawRectangleLinesEx((Rectangle){confirmBtnX, confirmBtnY, confirmBtnW, confirmBtnH},
                         confirmSelected ? 3 : 2, confirmBorderColor);

    const char *confirmText = "CONFIRM & CONTINUE";
    int ctw = (int)MeasureTextEx(g_font, confirmText, 18, 1).x;
    Color confirmTextColor = confirmSelected ? WHITE : COLOR_TEXT_DIM;
    DrawTextEx(g_font, confirmText, (Vector2){confirmBtnX + confirmBtnW / 2 - ctw / 2, confirmBtnY + 11}, 18, 1, confirmTextColor);

    // Instructions
    DrawTextEx(g_font, "Scroll: Browse  Click: Buy  Down: Confirm Button",
               (Vector2){g_screenWidth / 2 - 175, confirmBtnY + confirmBtnH + 10}, 12, 1, COLOR_TEXT_DIM);

    // Potion inventory panel at bottom
    int invY = g_screenHeight - 85;
    DrawRectangle(15, invY - 5, 380, 80, (Color){15, 15, 25, 230});
    DrawRectangleLinesEx((Rectangle){15, invY - 5, 380, 80}, 1, COLOR_TEXT_DIM);

    DrawTextEx(g_font, "POTIONS", (Vector2){25, invY}, 14, 1, COLOR_TEXT);
    DrawTextEx(g_font, "UP: Select  DOWN: Use", (Vector2){25, invY + 15}, 10, 1, COLOR_TEXT_DIM);

    // Potion slots
    int slotStartX = 25;
    int slotY = invY + 32;
    for (int i = 0; i < MAX_INVENTORY_POTIONS; i++) {
        int px = slotStartX + i * 36;
        bool isSelected = (i == g_game.selectedPotion);
        bool hasPotion = g_game.inventory[i].active;

        DrawRectangle(px, slotY, 30, 30, hasPotion ? (Color){30, 30, 45, 230} : COLOR_UI_BG);
        Color border = isSelected ? COLOR_UPGRADE_SEL : COLOR_TEXT_DIM;
        DrawRectangleLinesEx((Rectangle){px, slotY, 30, 30}, isSelected ? 2 : 1, border);

        if (hasPotion) {
            PotionType type = g_game.inventory[i].type;
            Color c = GetPotionColor(type);

            if (isSelected) {
                DrawCircleV((Vector2){px + 15, slotY + 15}, 13, (Color){c.r, c.g, c.b, 60});
            }
            DrawCircleV((Vector2){px + 15, slotY + 15}, 10, c);

            const char *symbol = GetPotionSymbol(type);
            DrawTextEx(g_font, symbol, (Vector2){px + 11, slotY + 10}, 12, 0, WHITE);
        }
    }

    // Selected potion tooltip
    if (g_game.inventory[g_game.selectedPotion].active) {
        PotionType type = g_game.inventory[g_game.selectedPotion].type;
        Color c = GetPotionColor(type);
        int tooltipX = slotStartX + MAX_INVENTORY_POTIONS * 36 + 10;
        DrawTextEx(g_font, GetPotionName(type), (Vector2){tooltipX, slotY + 2}, 14, 1, c);
        DrawTextEx(g_font, GetPotionDesc(type), (Vector2){tooltipX, slotY + 16}, 11, 1, COLOR_TEXT_DIM);
    }

    // Active buffs panel
    bool hasBuffs = false;
    for (int i = 0; i < POTION_COUNT; i++) {
        if (g_game.buffs[i].active) { hasBuffs = true; break; }
    }

    if (hasBuffs) {
        int buffX = g_screenWidth - 210;
        DrawRectangle(buffX - 5, invY - 5, 200, 80, (Color){15, 15, 25, 230});
        DrawRectangleLinesEx((Rectangle){buffX - 5, invY - 5, 200, 80}, 1, COLOR_TEXT_DIM);

        DrawTextEx(g_font, "ACTIVE BUFFS", (Vector2){buffX, invY}, 14, 1, COLOR_TEXT);

        int by = invY + 20;
        for (int i = 0; i < POTION_COUNT; i++) {
            if (g_game.buffs[i].active) {
                Color c = GetPotionColor((PotionType)i);
                float ratio = g_game.buffs[i].timer / g_game.buffs[i].duration;
                int seconds = (int)g_game.buffs[i].timer;

                // Icon and name
                DrawCircleV((Vector2){buffX + 8, by + 6}, 6, c);
                const char *symbol = GetPotionSymbol((PotionType)i);
                DrawTextEx(g_font, symbol, (Vector2){buffX + 5, by + 2}, 8, 0, WHITE);
                DrawTextEx(g_font, GetPotionName((PotionType)i), (Vector2){buffX + 20, by}, 10, 1, c);

                // Timer bar
                DrawRectangle(buffX + 70, by, (int)(60 * ratio), 12, c);
                DrawRectangleLinesEx((Rectangle){buffX + 70, by, 60, 12}, 1, WHITE);

                // Time remaining
                char timeStr[8];
                snprintf(timeStr, sizeof(timeStr), "%ds", seconds);
                DrawTextEx(g_font, timeStr, (Vector2){buffX + 135, by + 1}, 10, 1, COLOR_TEXT);

                by += 18;
            }
        }
    }
}

static void DrawWeaponSelect(void) {
    // Draw SDK animated background
    if (g_bgSystemInitialized) {
        LlzBackgroundDraw();
    } else {
        DrawRectangle(0, 0, g_screenWidth, g_screenHeight, COLOR_BG);
    }

    // Dark overlay gradient for carousel area contrast
    DrawRectangleGradientV(0, 60, g_screenWidth, g_screenHeight - 100,
                           (Color){10, 12, 20, 180}, (Color){20, 22, 35, 180});

    float centerX = g_screenWidth / 2.0f;
    float centerY = g_screenHeight / 2.0f;
    float entrance = EaseOutBack(g_weaponSelectEntrance);

    // Title with glow (larger, more prominent)
    const char *title = "SELECT WEAPON";
    int titleFontSize = 48;
    Font titleFont = LlzFontGet(LLZ_FONT_UI, titleFontSize);
    int tw = (int)MeasureTextEx(titleFont, title, titleFontSize, 1).x;
    float titleY = 15 - (1.0f - entrance) * 40;

    // Title glow pulse
    float glowPulse = (sinf(g_game.bgTime * 3.0f) + 1.0f) * 0.5f;
    Color titleGlow = COLOR_PLAYER;
    titleGlow.a = (unsigned char)((60 + 40 * glowPulse) * entrance);
    DrawCircleGradient((int)centerX, (int)(titleY + 24), 250 * entrance, titleGlow, BLANK);

    // Title shadow and text
    Color shadow = {0, 0, 0, (unsigned char)(180 * entrance)};
    DrawTextEx(titleFont, title, (Vector2){centerX - tw / 2 + 2, titleY + 2}, titleFontSize, 1, shadow);
    DrawTextEx(titleFont, title, (Vector2){centerX - tw / 2, titleY}, titleFontSize, 1, titleGlow);
    DrawTextEx(titleFont, title, (Vector2){centerX - tw / 2, titleY}, titleFontSize, 1, COLOR_PLAYER);

    // Weapon card data
    LlzGemColor weaponGems[] = {LLZ_GEM_RUBY, LLZ_GEM_TOPAZ, LLZ_GEM_AMETHYST, LLZ_GEM_SAPPHIRE, LLZ_GEM_DIAMOND};
    LlzShapeType weaponShapes[] = {LLZ_SHAPE_TRIANGLE, LLZ_SHAPE_CIRCLE, LLZ_SHAPE_STAR, LLZ_SHAPE_HEXAGON, LLZ_SHAPE_TALL_DIAMOND};

    // Card dimensions (large like Bejeweled)
    float baseCardWidth = 160;
    float baseCardHeight = 220;
    float cardSpacing = 140;

    // Sort cards by distance for proper z-ordering (farthest first)
    int drawOrder[STARTING_WEAPON_COUNT];
    float distances[STARTING_WEAPON_COUNT];
    for (int i = 0; i < STARTING_WEAPON_COUNT; i++) {
        float offset = (float)i - g_weaponCarouselPos;
        distances[i] = fabsf(offset);
        drawOrder[i] = i;
    }
    for (int i = 0; i < STARTING_WEAPON_COUNT - 1; i++) {
        for (int j = i + 1; j < STARTING_WEAPON_COUNT; j++) {
            if (distances[drawOrder[i]] < distances[drawOrder[j]]) {
                int temp = drawOrder[i];
                drawOrder[i] = drawOrder[j];
                drawOrder[j] = temp;
            }
        }
    }

    // Draw cards in sorted order (back to front)
    for (int d = 0; d < STARTING_WEAPON_COUNT; d++) {
        int i = drawOrder[d];

        // Calculate position offset from center
        float offset = (float)i - g_weaponCarouselPos;
        float absOffset = fabsf(offset);

        // Scale based on distance from center (center=1.0, far=0.55)
        float scale;
        if (absOffset < 0.1f) {
            scale = 1.0f;
        } else if (absOffset < 1.5f) {
            scale = 1.0f - 0.3f * absOffset;
        } else {
            scale = 0.55f;
        }

        // Entrance animation scale
        float cardEntrance = Clampf((g_weaponSelectEntrance - 0.1f) * 2.0f, 0, 1);
        scale *= EaseOutBack(cardEntrance);

        // Selected card pulse
        bool isSelected = (i == g_game.weaponSelectIndex);
        float selPulse = isSelected ? (sinf(g_game.bgTime * 6.0f) + 1.0f) * 0.5f * 0.05f : 0.0f;
        scale += selPulse;

        // Alpha based on distance
        float alpha = 1.0f;
        if (absOffset > 1.5f) {
            alpha = 0.4f;
        } else if (absOffset > 0.5f) {
            alpha = 1.0f - 0.4f * (absOffset - 0.5f);
        }
        alpha *= entrance;

        // Card position
        float cardWidth = baseCardWidth * scale;
        float cardHeight = baseCardHeight * scale;
        float cardX = centerX + offset * cardSpacing - cardWidth / 2.0f;
        float cardY = centerY - cardHeight / 2.0f + 15;

        // Glow effect for selected card
        float glowIntensity = g_weaponCardGlow[i];
        if (glowIntensity > 0.01f) {
            Color glowColor = LlzGetGemColor(weaponGems[i]);
            float gPulse = (sinf(g_game.bgTime * 6.0f) + 1.0f) * 0.5f;
            glowColor.a = (unsigned char)((80 + 60 * gPulse) * glowIntensity * alpha);
            DrawRectangleRounded((Rectangle){cardX - 10, cardY - 10, cardWidth + 20, cardHeight + 20},
                                0.12f, 8, glowColor);
        }

        // Card background
        Color cardBg = isSelected ? (Color){45, 55, 80, (unsigned char)(255 * alpha)}
                                  : (Color){30, 35, 50, (unsigned char)(255 * alpha)};
        DrawRectangleRounded((Rectangle){cardX, cardY, cardWidth, cardHeight}, 0.12f, 8, cardBg);

        // Card border
        Color borderColor = LlzGetGemColor(weaponGems[i]);
        borderColor.a = (unsigned char)((isSelected ? 255 : 120) * alpha);
        DrawRectangleRoundedLines((Rectangle){cardX, cardY, cardWidth, cardHeight}, 0.12f, 8, borderColor);

        // Weapon name at top
        float nameY = cardY + 20 * scale;
        int nameFontSize = (int)(28 * scale);
        if (nameFontSize < 12) nameFontSize = 12;
        Font nameFont = LlzFontGet(LLZ_FONT_UI, nameFontSize);
        Vector2 nameSize = MeasureTextEx(nameFont, WEAPON_NAMES[i], nameFontSize, 1);
        Color nameColor = LlzGetGemColor(weaponGems[i]);
        nameColor.a = (unsigned char)(255 * alpha);
        DrawTextEx(nameFont, WEAPON_NAMES[i], (Vector2){cardX + cardWidth / 2 - nameSize.x / 2, nameY}, nameFontSize, 1, nameColor);

        // Large weapon icon (gem shape) in center
        float iconY = cardY + cardHeight * 0.45f;
        float iconSize = 45 * scale;
        float iconBob = isSelected ? sinf(g_game.bgTime * 2.5f) * 4.0f : 0.0f;
        LlzDrawGemShape(weaponShapes[i], cardX + cardWidth / 2, iconY + iconBob, iconSize, weaponGems[i]);

        // Inner highlight on gem
        Color innerColor = LlzGetGemColorLight(weaponGems[i]);
        innerColor.a = (unsigned char)((isSelected ? 180 : 100) * alpha);
        DrawCircleV((Vector2){cardX + cardWidth / 2 - 8 * scale, iconY - 8 * scale + iconBob}, 6 * scale, innerColor);

        // Description at bottom
        if (alpha > 0.3f) {
            float descY = cardY + cardHeight * 0.72f;
            int descFontSize = (int)(16 * scale);
            if (descFontSize < 10) descFontSize = 10;
            Font descFont = LlzFontGet(LLZ_FONT_UI, descFontSize);
            Vector2 descSize = MeasureTextEx(descFont, WEAPON_DESCS[i], descFontSize, 1);
            Color descColor = isSelected ? (Color){240, 240, 250, (unsigned char)(255 * alpha)}
                                         : (Color){180, 185, 200, (unsigned char)(200 * alpha)};
            DrawTextEx(descFont, WEAPON_DESCS[i], (Vector2){cardX + cardWidth / 2 - descSize.x / 2, descY}, descFontSize, 1, descColor);
        }
    }

    // Instructions at bottom
    float instrAlpha = Clampf((g_weaponSelectEntrance - 0.4f) * 3.0f, 0, 1);
    const char *instructions = "SCROLL TO SELECT  -  PRESS TO START";
    int instrFontSize = 18;
    Font instrFont = LlzFontGet(LLZ_FONT_UI, instrFontSize);
    Vector2 instrSize = MeasureTextEx(instrFont, instructions, instrFontSize, 1);
    float instrPulse = 150 + 105 * sinf(g_game.bgTime * 2.5f);
    Color instrColor = {240, 240, 250, (unsigned char)(instrPulse * instrAlpha)};
    DrawTextEx(instrFont, instructions, (Vector2){centerX - instrSize.x / 2, g_screenHeight - 45}, instrFontSize, 1, instrColor);

    // Weapon indicator dots
    float dotY = g_screenHeight - 75;
    float dotSpacing = 20;
    float totalDotWidth = (STARTING_WEAPON_COUNT - 1) * dotSpacing;
    float dotStartX = centerX - totalDotWidth / 2.0f;

    for (int i = 0; i < STARTING_WEAPON_COUNT; i++) {
        float dotX = dotStartX + i * dotSpacing;
        bool isSelected = (i == g_game.weaponSelectIndex);
        Color dotColor = isSelected ? LlzGetGemColor(weaponGems[i]) : (Color){80, 85, 100, 200};
        float dotSize = isSelected ? 6.0f : 4.0f;
        dotColor.a = (unsigned char)(dotColor.a * instrAlpha);
        DrawCircleV((Vector2){dotX, dotY}, dotSize, dotColor);
    }

    // Hint about unlocking more
    const char *hint2 = "More weapons unlock during gameplay!";
    Font hint2Font = LlzFontGet(LLZ_FONT_UI, 14);
    int hw2 = (int)MeasureTextEx(hint2Font, hint2, 14, 1).x;
    Color hint2Color = COLOR_XP_BAR;
    hint2Color.a = (unsigned char)(180 * instrAlpha);
    DrawTextEx(hint2Font, hint2, (Vector2){centerX - hw2 / 2, g_screenHeight - 22}, 14, 1, hint2Color);
}

static void DrawClassSelect(void) {
    // Draw SDK animated background
    if (g_bgSystemInitialized) {
        LlzBackgroundDraw();
    } else {
        DrawRectangle(0, 0, g_screenWidth, g_screenHeight, COLOR_BG);
    }

    // Dark overlay gradient for carousel area contrast
    DrawRectangleGradientV(0, 60, g_screenWidth, g_screenHeight - 100,
                           (Color){10, 12, 20, 180}, (Color){20, 22, 35, 180});

    float centerX = g_screenWidth / 2.0f;
    float centerY = g_screenHeight / 2.0f;
    float entrance = EaseOutBack(g_classSelectEntrance);

    // Title with glow
    const char *title = "SELECT CLASS";
    int titleFontSize = 48;
    Font titleFont = LlzFontGet(LLZ_FONT_UI, titleFontSize);
    int tw = (int)MeasureTextEx(titleFont, title, titleFontSize, 1).x;
    float titleY = 15 - (1.0f - entrance) * 40;

    // Title glow pulse
    float glowPulse = (sinf(g_game.bgTime * 3.0f) + 1.0f) * 0.5f;
    Color titleGlow = COLOR_PLAYER;
    titleGlow.a = (unsigned char)((60 + 40 * glowPulse) * entrance);
    DrawCircleGradient((int)centerX, (int)(titleY + 24), 250 * entrance, titleGlow, BLANK);

    // Title shadow and text
    Color shadow = {0, 0, 0, (unsigned char)(180 * entrance)};
    DrawTextEx(titleFont, title, (Vector2){centerX - tw / 2 + 2, titleY + 2}, titleFontSize, 1, shadow);
    DrawTextEx(titleFont, title, (Vector2){centerX - tw / 2, titleY}, titleFontSize, 1, titleGlow);
    DrawTextEx(titleFont, title, (Vector2){centerX - tw / 2, titleY}, titleFontSize, 1, COLOR_PLAYER);

    // Card dimensions
    float baseCardWidth = 150;
    float baseCardHeight = 260;
    float cardSpacing = 130;

    // Sort cards by distance for proper z-ordering (farthest first)
    int drawOrder[CLASS_COUNT];
    float distances[CLASS_COUNT];
    for (int i = 0; i < CLASS_COUNT; i++) {
        float offset = (float)i - g_classCarouselPos;
        distances[i] = fabsf(offset);
        drawOrder[i] = i;
    }
    for (int i = 0; i < CLASS_COUNT - 1; i++) {
        for (int j = i + 1; j < CLASS_COUNT; j++) {
            if (distances[drawOrder[i]] < distances[drawOrder[j]]) {
                int temp = drawOrder[i];
                drawOrder[i] = drawOrder[j];
                drawOrder[j] = temp;
            }
        }
    }

    // Draw cards in sorted order (back to front)
    for (int d = 0; d < CLASS_COUNT; d++) {
        int i = drawOrder[d];
        const ClassStats *cls = &CLASS_STATS[i];

        // Calculate position offset from center
        float offset = (float)i - g_classCarouselPos;
        float absOffset = fabsf(offset);

        // Scale based on distance from center
        float scale;
        if (absOffset < 0.1f) {
            scale = 1.0f;
        } else if (absOffset < 1.5f) {
            scale = 1.0f - 0.25f * absOffset;
        } else {
            scale = 0.6f;
        }

        // Entrance animation scale
        float cardEntrance = Clampf((g_classSelectEntrance - 0.1f) * 2.0f, 0, 1);
        scale *= EaseOutBack(cardEntrance);

        // Selected card pulse
        bool isSelected = (i == g_game.classSelectIndex);
        float selPulse = isSelected ? (sinf(g_game.bgTime * 6.0f) + 1.0f) * 0.5f * 0.05f : 0.0f;
        scale += selPulse;

        // Alpha based on distance
        float alpha = 1.0f;
        if (absOffset > 1.5f) {
            alpha = 0.4f;
        } else if (absOffset > 0.5f) {
            alpha = 1.0f - 0.4f * (absOffset - 0.5f);
        }
        alpha *= entrance;

        // Card position
        float cardWidth = baseCardWidth * scale;
        float cardHeight = baseCardHeight * scale;
        float cardX = centerX + offset * cardSpacing - cardWidth / 2.0f;
        float cardY = centerY - cardHeight / 2.0f + 20;

        // Glow effect for selected card
        float glowIntensity = g_classCardGlow[i];
        if (glowIntensity > 0.01f) {
            Color glowColor = cls->classColor;
            float gPulse = (sinf(g_game.bgTime * 6.0f) + 1.0f) * 0.5f;
            glowColor.a = (unsigned char)((80 + 60 * gPulse) * glowIntensity * alpha);
            DrawRectangleRounded((Rectangle){cardX - 10, cardY - 10, cardWidth + 20, cardHeight + 20},
                                0.12f, 8, glowColor);
        }

        // Card background
        Color cardBg = isSelected ? (Color){45, 55, 80, (unsigned char)(255 * alpha)}
                                  : (Color){30, 35, 50, (unsigned char)(255 * alpha)};
        DrawRectangleRounded((Rectangle){cardX, cardY, cardWidth, cardHeight}, 0.12f, 8, cardBg);

        // Card border with class color
        Color borderColor = cls->classColor;
        borderColor.a = (unsigned char)((isSelected ? 255 : 120) * alpha);
        DrawRectangleRoundedLines((Rectangle){cardX, cardY, cardWidth, cardHeight}, 0.12f, 8, borderColor);

        // Class name at top
        float nameY = cardY + 15 * scale;
        int nameFontSize = (int)(24 * scale);
        if (nameFontSize < 10) nameFontSize = 10;
        Font nameFont = LlzFontGet(LLZ_FONT_UI, nameFontSize);
        Vector2 nameSize = MeasureTextEx(nameFont, cls->name, nameFontSize, 1);
        Color nameColor = cls->classColor;
        nameColor.a = (unsigned char)(255 * alpha);
        DrawTextEx(nameFont, cls->name, (Vector2){cardX + cardWidth / 2 - nameSize.x / 2, nameY}, nameFontSize, 1, nameColor);

        // Class icon (player shape with class color) in upper area
        float iconY = cardY + cardHeight * 0.25f;
        float iconSize = 25 * scale;
        float iconBob = isSelected ? sinf(g_game.bgTime * 2.5f) * 3.0f : 0.0f;
        Color iconColor = cls->classColor;
        iconColor.a = (unsigned char)(255 * alpha);
        DrawCircleV((Vector2){cardX + cardWidth / 2, iconY + iconBob}, iconSize, iconColor);
        // Direction arrow
        Color arrowColor = {255, 255, 255, (unsigned char)(200 * alpha)};
        DrawTriangle(
            (Vector2){cardX + cardWidth / 2, iconY + iconBob - iconSize * 0.8f},
            (Vector2){cardX + cardWidth / 2 - iconSize * 0.4f, iconY + iconBob - iconSize * 0.2f},
            (Vector2){cardX + cardWidth / 2 + iconSize * 0.4f, iconY + iconBob - iconSize * 0.2f},
            arrowColor
        );

        // Stats section (only on visible cards)
        if (alpha > 0.3f) {
            float statsY = cardY + cardHeight * 0.42f;
            int statFontSize = (int)(14 * scale);
            if (statFontSize < 8) statFontSize = 8;
            Font statFont = LlzFontGet(LLZ_FONT_UI, statFontSize);
            float lineHeight = statFontSize + 4 * scale;
            Color statLabelColor = {180, 185, 200, (unsigned char)(200 * alpha)};
            Color statValueColor = {240, 240, 250, (unsigned char)(255 * alpha)};

            // HP stat
            char hpText[32];
            snprintf(hpText, sizeof(hpText), "HP: %d", cls->baseHP);
            Color hpColor = cls->baseHP > 100 ? (Color){100, 255, 100, (unsigned char)(255 * alpha)} :
                           (cls->baseHP < 100 ? (Color){255, 150, 100, (unsigned char)(255 * alpha)} : statValueColor);
            DrawTextEx(statFont, hpText, (Vector2){cardX + 10 * scale, statsY}, statFontSize, 1, hpColor);

            // Speed stat
            char spdText[32];
            snprintf(spdText, sizeof(spdText), "SPD: %.0f%%", cls->speedMultiplier * 100);
            Color spdColor = cls->speedMultiplier > 1.0f ? (Color){100, 255, 100, (unsigned char)(255 * alpha)} :
                            (cls->speedMultiplier < 1.0f ? (Color){255, 150, 100, (unsigned char)(255 * alpha)} : statValueColor);
            DrawTextEx(statFont, spdText, (Vector2){cardX + 10 * scale, statsY + lineHeight}, statFontSize, 1, spdColor);

            // Armor stat
            char armText[32];
            snprintf(armText, sizeof(armText), "ARM: %.0f%%", cls->armorPercent);
            Color armColor = cls->armorPercent > 0 ? (Color){100, 255, 100, (unsigned char)(255 * alpha)} : statLabelColor;
            DrawTextEx(statFont, armText, (Vector2){cardX + 10 * scale, statsY + lineHeight * 2}, statFontSize, 1, armColor);

            // XP multiplier stat
            char xpText[32];
            snprintf(xpText, sizeof(xpText), "XP: %.0f%%", cls->xpMultiplier * 100);
            Color xpColor = cls->xpMultiplier > 1.0f ? (Color){100, 255, 100, (unsigned char)(255 * alpha)} :
                           (cls->xpMultiplier < 1.0f ? (Color){255, 150, 100, (unsigned char)(255 * alpha)} : statValueColor);
            DrawTextEx(statFont, xpText, (Vector2){cardX + 10 * scale, statsY + lineHeight * 3}, statFontSize, 1, xpColor);

            // Preferred weapon
            float weaponY = cardY + cardHeight * 0.78f;
            int weaponFontSize = (int)(12 * scale);
            if (weaponFontSize < 8) weaponFontSize = 8;
            Font weaponFont = LlzFontGet(LLZ_FONT_UI, weaponFontSize);

            const char *weaponName = WEAPON_NAMES[cls->preferredWeapon];
            char bonusText[48];
            if (cls->weaponDamageBonus > 0) {
                snprintf(bonusText, sizeof(bonusText), "%s +%.0f%%", weaponName, cls->weaponDamageBonus);
            } else {
                snprintf(bonusText, sizeof(bonusText), "%s", weaponName);
            }
            Vector2 bonusSize = MeasureTextEx(weaponFont, bonusText, weaponFontSize, 1);
            Color bonusColor = cls->weaponDamageBonus > 0 ? (Color){255, 215, 100, (unsigned char)(255 * alpha)} : statLabelColor;
            DrawTextEx(weaponFont, bonusText, (Vector2){cardX + cardWidth / 2 - bonusSize.x / 2, weaponY}, weaponFontSize, 1, bonusColor);

            // Description at bottom
            float descY = cardY + cardHeight * 0.88f;
            int descFontSize = (int)(11 * scale);
            if (descFontSize < 8) descFontSize = 8;
            Font descFont = LlzFontGet(LLZ_FONT_UI, descFontSize);
            Vector2 descSize = MeasureTextEx(descFont, cls->description, descFontSize, 1);
            Color descColor = isSelected ? (Color){220, 220, 230, (unsigned char)(220 * alpha)}
                                         : (Color){160, 165, 180, (unsigned char)(180 * alpha)};
            // Center if fits, otherwise left-align with padding
            float descX = (descSize.x < cardWidth - 10 * scale)
                        ? (cardX + cardWidth / 2 - descSize.x / 2)
                        : (cardX + 5 * scale);
            DrawTextEx(descFont, cls->description, (Vector2){descX, descY}, descFontSize, 1, descColor);
        }
    }

    // Instructions at bottom
    float instrAlpha = Clampf((g_classSelectEntrance - 0.4f) * 3.0f, 0, 1);
    const char *instructions = "SCROLL TO SELECT  -  PRESS TO CONFIRM";
    int instrFontSize = 18;
    Font instrFont = LlzFontGet(LLZ_FONT_UI, instrFontSize);
    Vector2 instrSize = MeasureTextEx(instrFont, instructions, instrFontSize, 1);
    float instrPulse = 150 + 105 * sinf(g_game.bgTime * 2.5f);
    Color instrColor = {240, 240, 250, (unsigned char)(instrPulse * instrAlpha)};
    DrawTextEx(instrFont, instructions, (Vector2){centerX - instrSize.x / 2, g_screenHeight - 45}, instrFontSize, 1, instrColor);

    // Class indicator dots
    float dotY = g_screenHeight - 75;
    float dotSpacing = 20;
    float totalDotWidth = (CLASS_COUNT - 1) * dotSpacing;
    float dotStartX = centerX - totalDotWidth / 2.0f;

    for (int i = 0; i < CLASS_COUNT; i++) {
        float dotX = dotStartX + i * dotSpacing;
        bool isSelected = (i == g_game.classSelectIndex);
        Color dotColor = isSelected ? CLASS_STATS[i].classColor : (Color){80, 85, 100, 200};
        float dotSize = isSelected ? 6.0f : 4.0f;
        dotColor.a = (unsigned char)(dotColor.a * instrAlpha);
        DrawCircleV((Vector2){dotX, dotY}, dotSize, dotColor);
    }
}

static void DrawMenu(void) {
    // Draw SDK animated background
    if (g_bgSystemInitialized) {
        LlzBackgroundDraw();
    } else {
        DrawRectangle(0, 0, g_screenWidth, g_screenHeight, COLOR_BG);
    }

    // Entrance animation easing
    float entrance = EaseOutBack(g_menuEntranceTime);

    // Title with glow effect
    const char *title = "LLZ SURVIVORS";
    Font titleFont = LlzFontGet(LLZ_FONT_UI, 48);
    int tw = (int)MeasureTextEx(titleFont, title, 48, 1).x;
    float titleY = 100 - (1.0f - entrance) * 50;  // Slide in from top

    // Pulsing glow behind title
    float glowIntensity = 0.5f + 0.5f * sinf(g_menuTitleGlow * 2.0f);
    Color glowColor = COLOR_PLAYER;
    glowColor.a = (unsigned char)(100 * glowIntensity * entrance);
    DrawCircleGradient(g_screenWidth / 2, (int)(titleY + 20), 200 * entrance, glowColor, BLANK);

    // Secondary outer glow
    Color outerGlow = LlzGetGemColor(LLZ_GEM_SAPPHIRE);
    outerGlow.a = (unsigned char)(40 * glowIntensity * entrance);
    DrawCircleGradient(g_screenWidth / 2, (int)(titleY + 20), 300 * entrance, outerGlow, BLANK);

    // Title shadow
    Color shadow = {0, 0, 0, (unsigned char)(150 * entrance)};
    DrawTextEx(titleFont, title, (Vector2){g_screenWidth / 2 - tw / 2 + 3, titleY + 3}, 48, 1, shadow);

    // Title text
    Color titleColor = COLOR_PLAYER;
    titleColor.a = (unsigned char)(255 * entrance);
    DrawTextEx(titleFont, title, (Vector2){g_screenWidth / 2 - tw / 2, titleY}, 48, 1, titleColor);

    // Menu options with hover animations
    const char *options[] = {"Start Game", "Exit"};
    int baseY = 220;
    for (int i = 0; i < 2; i++) {
        // Staggered entrance (second button delays slightly)
        float buttonEntrance = Clampf((g_menuEntranceTime - i * 0.1f) * 2.0f, 0, 1);
        buttonEntrance = EaseOutBack(buttonEntrance);

        float scale = g_menuButtonScale[i];
        int fontSize = (int)(28 * scale);
        Font btnFont = LlzFontGet(LLZ_FONT_UI, fontSize);
        int ow = (int)MeasureTextEx(btnFont, options[i], fontSize, 1).x;

        float offsetX = (1.0f - buttonEntrance) * -100;  // Slide in from left
        int x = g_screenWidth / 2 - ow / 2 + (int)offsetX;
        int y = baseY + i * 55;

        bool selected = (i == g_game.menuIndex);

        // Selection glow
        if (selected && buttonEntrance > 0.5f) {
            float selGlow = 0.6f + 0.4f * sinf(g_menuTitleGlow * 4.0f);
            Color selColor = COLOR_UPGRADE_SEL;
            selColor.a = (unsigned char)(60 * selGlow * buttonEntrance);
            DrawCircleGradient(g_screenWidth / 2 + (int)offsetX, y + fontSize / 2, 80 * scale, selColor, BLANK);
        }

        // Text shadow
        Color btnShadow = {0, 0, 0, (unsigned char)(120 * buttonEntrance)};
        DrawTextEx(btnFont, options[i], (Vector2){x + 2, y + 2}, fontSize, 1, btnShadow);

        // Text
        Color c = selected ? COLOR_UPGRADE_SEL : COLOR_TEXT_DIM;
        c.a = (unsigned char)(255 * buttonEntrance);
        DrawTextEx(btnFont, options[i], (Vector2){x, y}, fontSize, 1, c);

        // Selection indicator (animated gem)
        if (selected) {
            float indicatorBob = sinf(g_menuTitleGlow * 3.0f) * 3.0f;
            LlzDrawGemShape(LLZ_SHAPE_DIAMOND, x - 25, y + fontSize / 2 + indicatorBob, 8 * scale, LLZ_GEM_SAPPHIRE);
        }
    }

    // Controls hint with fade-in
    float hintAlpha = Clampf((g_menuEntranceTime - 0.5f) * 2.0f, 0, 1);
    const char *controls = "Mouse: Aim | Click: Toggle Move | Hold+Drag: Auto-Move";
    Font hintFont = LlzFontGet(LLZ_FONT_UI, 14);
    int cw = (int)MeasureTextEx(hintFont, controls, 14, 1).x;
    Color hintColor = COLOR_TEXT_DIM;
    hintColor.a = (unsigned char)(200 * hintAlpha);
    DrawTextEx(hintFont, controls, (Vector2){g_screenWidth / 2 - cw / 2, g_screenHeight - 50}, 14, 1, hintColor);
}

static void DrawGameOver(void) {
    // Dark overlay with entrance fade
    float entrance = EaseOutQuad(g_gameOverEntrance);
    Color overlayColor = {0, 0, 0, (unsigned char)(220 * entrance)};
    DrawRectangle(0, 0, g_screenWidth, g_screenHeight, overlayColor);

    // Title with dramatic entrance
    const char *title = "GAME OVER";
    Font titleFont = LlzFontGet(LLZ_FONT_UI, 48);
    int tw = (int)MeasureTextEx(titleFont, title, 48, 1).x;

    // Title slides down with scale effect
    float titleProgress = Clampf(g_gameOverEntrance * 2.0f, 0, 1);
    float titleScale = 0.5f + 0.5f * EaseOutBack(titleProgress);
    float titleY = 80 - (1.0f - titleProgress) * 50;

    // Pulsing red glow behind title
    float glowPulse = 0.5f + 0.5f * sinf(g_game.bgTime * 3.0f);
    Color titleGlow = COLOR_WALKER;
    titleGlow.a = (unsigned char)(80 * glowPulse * titleProgress);
    DrawCircleGradient(g_screenWidth / 2, (int)(titleY + 20), 200 * titleScale, titleGlow, BLANK);

    // Title shadow
    Color shadow = {0, 0, 0, (unsigned char)(200 * titleProgress)};
    int scaledSize = (int)(48 * titleScale);
    int scaledWidth = (int)(tw * titleScale);
    DrawTextEx(titleFont, title, (Vector2){g_screenWidth / 2 - scaledWidth / 2 + 3, titleY + 3}, scaledSize, 1, shadow);

    // Title text
    Color titleColor = COLOR_WALKER;
    titleColor.a = (unsigned char)(255 * titleProgress);
    DrawTextEx(titleFont, title, (Vector2){g_screenWidth / 2 - scaledWidth / 2, titleY}, scaledSize, 1, titleColor);

    // Stats with count-up animation
    int statsY = 160;
    Font statsFont = LlzFontGet(LLZ_FONT_UI, 24);
    char buf[64];

    // Stat 1: Time survived
    float stat1Progress = Clampf((g_statCountUp - 0.0f) * 3.0f, 0, 1);
    if (stat1Progress > 0) {
        int displayMins = (int)(g_displayedTime) / 60;
        int displaySecs = (int)(g_displayedTime) % 60;
        snprintf(buf, sizeof(buf), "Survived: %d:%02d", displayMins, displaySecs);

        // Slide in from left
        float offsetX = (1.0f - EaseOutQuad(stat1Progress)) * -100;
        int bw = (int)MeasureTextEx(statsFont, buf, 24, 1).x;
        Color statColor = COLOR_TEXT;
        statColor.a = (unsigned char)(255 * stat1Progress);

        // Gem icon
        if (stat1Progress > 0.5f) {
            LlzDrawGemShape(LLZ_SHAPE_CIRCLE, g_screenWidth / 2 - bw / 2 - 25 + offsetX, statsY + 12, 8, LLZ_GEM_EMERALD);
        }
        DrawTextEx(statsFont, buf, (Vector2){g_screenWidth / 2 - bw / 2 + offsetX, statsY}, 24, 1, statColor);
    }

    // Stat 2: Kills
    float stat2Progress = Clampf((g_statCountUp - 0.15f) * 3.0f, 0, 1);
    if (stat2Progress > 0) {
        snprintf(buf, sizeof(buf), "Kills: %d", g_displayedKills);

        float offsetX = (1.0f - EaseOutQuad(stat2Progress)) * 100;  // From right
        int bw = (int)MeasureTextEx(statsFont, buf, 24, 1).x;
        Color statColor = COLOR_TEXT;
        statColor.a = (unsigned char)(255 * stat2Progress);

        if (stat2Progress > 0.5f) {
            LlzDrawGemShape(LLZ_SHAPE_TRIANGLE, g_screenWidth / 2 - bw / 2 - 25 + offsetX, statsY + 52, 8, LLZ_GEM_RUBY);
        }
        DrawTextEx(statsFont, buf, (Vector2){g_screenWidth / 2 - bw / 2 + offsetX, statsY + 40}, 24, 1, statColor);
    }

    // Stat 3: Wave and Level
    float stat3Progress = Clampf((g_statCountUp - 0.3f) * 3.0f, 0, 1);
    if (stat3Progress > 0) {
        snprintf(buf, sizeof(buf), "Wave: %d  Level: %d", g_game.highestWave + 1, g_game.player.level);

        float offsetX = (1.0f - EaseOutQuad(stat3Progress)) * -100;
        int bw = (int)MeasureTextEx(statsFont, buf, 24, 1).x;
        Color statColor = COLOR_TEXT;
        statColor.a = (unsigned char)(255 * stat3Progress);

        if (stat3Progress > 0.5f) {
            LlzDrawGemShape(LLZ_SHAPE_STAR, g_screenWidth / 2 - bw / 2 - 25 + offsetX, statsY + 92, 8, LLZ_GEM_TOPAZ);
        }
        DrawTextEx(statsFont, buf, (Vector2){g_screenWidth / 2 - bw / 2 + offsetX, statsY + 80}, 24, 1, statColor);
    }

    // Continue prompt (fades in last)
    float promptProgress = Clampf((g_statCountUp - 0.6f) * 2.5f, 0, 1);
    if (promptProgress > 0) {
        Font promptFont = LlzFontGet(LLZ_FONT_UI, 18);
        const char *prompt = "Press any button to continue";
        int pw = (int)MeasureTextEx(promptFont, prompt, 18, 1).x;

        // Pulsing alpha
        float pulse = 0.6f + 0.4f * sinf(g_game.bgTime * 3.0f);
        Color promptColor = COLOR_TEXT_DIM;
        promptColor.a = (unsigned char)(200 * promptProgress * pulse);

        DrawTextEx(promptFont, prompt, (Vector2){g_screenWidth / 2 - pw / 2, g_screenHeight - 60}, 18, 1, promptColor);
    }
}

static void DrawVictory(void) {
    // Gold-tinted overlay with entrance fade
    float entrance = EaseOutQuad(g_gameOverEntrance);
    Color overlayColor = {20, 15, 0, (unsigned char)(220 * entrance)};
    DrawRectangle(0, 0, g_screenWidth, g_screenHeight, overlayColor);

    // Animated gold particles in background
    float time = g_game.bgTime;
    for (int i = 0; i < 20; i++) {
        float x = fmodf(i * 47.0f + time * 30.0f, g_screenWidth + 40) - 20;
        float y = fmodf(i * 31.0f + time * 20.0f + i * 17.0f, g_screenHeight + 40) - 20;
        float size = 3.0f + sinf(time * 2.0f + i) * 2.0f;
        float alpha = (0.3f + 0.3f * sinf(time * 3.0f + i * 0.5f)) * entrance;
        Color starColor = {255, 215, 0, (unsigned char)(150 * alpha)};
        DrawCircleV((Vector2){x, y}, size, starColor);
    }

    // "VICTORY!" title with dramatic entrance
    const char *title = "VICTORY!";
    Font titleFont = LlzFontGet(LLZ_FONT_UI, 56);
    int tw = (int)MeasureTextEx(titleFont, title, 56, 1).x;

    float titleProgress = Clampf(g_gameOverEntrance * 2.0f, 0, 1);
    float titleScale = 0.5f + 0.5f * EaseOutBack(titleProgress);
    float titleY = 60 - (1.0f - titleProgress) * 60;

    // Pulsing gold glow behind title
    float glowPulse = 0.5f + 0.5f * sinf(g_game.bgTime * 4.0f);
    Color titleGlow = {255, 215, 0, (unsigned char)(120 * glowPulse * titleProgress)};
    DrawCircleGradient(g_screenWidth / 2, (int)(titleY + 28), 280 * titleScale, titleGlow, BLANK);

    // Secondary outer glow (white)
    Color outerGlow = {255, 255, 200, (unsigned char)(60 * glowPulse * titleProgress)};
    DrawCircleGradient(g_screenWidth / 2, (int)(titleY + 28), 350 * titleScale, outerGlow, BLANK);

    // Title shadow
    Color shadow = {0, 0, 0, (unsigned char)(200 * titleProgress)};
    int scaledSize = (int)(56 * titleScale);
    int scaledWidth = (int)(tw * titleScale);
    DrawTextEx(titleFont, title, (Vector2){g_screenWidth / 2 - scaledWidth / 2 + 3, titleY + 3}, scaledSize, 1, shadow);

    // Title text (gold)
    Color titleColor = {255, 215, 0, (unsigned char)(255 * titleProgress)};
    DrawTextEx(titleFont, title, (Vector2){g_screenWidth / 2 - scaledWidth / 2, titleY}, scaledSize, 1, titleColor);

    // Subtitle
    float subProgress = Clampf((g_gameOverEntrance - 0.2f) * 3.0f, 0, 1);
    if (subProgress > 0) {
        const char *subtitle = "LEVEL 20 REACHED!";
        Font subFont = LlzFontGet(LLZ_FONT_UI, 24);
        int sw = (int)MeasureTextEx(subFont, subtitle, 24, 1).x;
        Color subColor = {255, 255, 200, (unsigned char)(255 * subProgress)};
        DrawTextEx(subFont, subtitle, (Vector2){g_screenWidth / 2 - sw / 2, titleY + 60}, 24, 1, subColor);
    }

    // Stats with count-up animation (reusing game over animation system)
    int statsY = 160;
    Font statsFont = LlzFontGet(LLZ_FONT_UI, 24);
    char buf[64];

    // Stat 1: Time survived
    float stat1Progress = Clampf((g_statCountUp - 0.0f) * 3.0f, 0, 1);
    if (stat1Progress > 0) {
        int displayMins = (int)(g_displayedTime) / 60;
        int displaySecs = (int)(g_displayedTime) % 60;
        snprintf(buf, sizeof(buf), "Completed in: %d:%02d", displayMins, displaySecs);

        float offsetX = (1.0f - EaseOutQuad(stat1Progress)) * -100;
        int bw = (int)MeasureTextEx(statsFont, buf, 24, 1).x;
        Color statColor = {255, 255, 200, (unsigned char)(255 * stat1Progress)};

        if (stat1Progress > 0.5f) {
            LlzDrawGemShape(LLZ_SHAPE_CIRCLE, g_screenWidth / 2 - bw / 2 - 25 + offsetX, statsY + 12, 8, LLZ_GEM_TOPAZ);
        }
        DrawTextEx(statsFont, buf, (Vector2){g_screenWidth / 2 - bw / 2 + offsetX, statsY}, 24, 1, statColor);
    }

    // Stat 2: Kills
    float stat2Progress = Clampf((g_statCountUp - 0.15f) * 3.0f, 0, 1);
    if (stat2Progress > 0) {
        snprintf(buf, sizeof(buf), "Enemies Slain: %d", g_displayedKills);

        float offsetX = (1.0f - EaseOutQuad(stat2Progress)) * 100;
        int bw = (int)MeasureTextEx(statsFont, buf, 24, 1).x;
        Color statColor = {255, 255, 200, (unsigned char)(255 * stat2Progress)};

        if (stat2Progress > 0.5f) {
            LlzDrawGemShape(LLZ_SHAPE_TRIANGLE, g_screenWidth / 2 - bw / 2 - 25 + offsetX, statsY + 52, 8, LLZ_GEM_RUBY);
        }
        DrawTextEx(statsFont, buf, (Vector2){g_screenWidth / 2 - bw / 2 + offsetX, statsY + 40}, 24, 1, statColor);
    }

    // Stat 3: Highest Wave
    float stat3Progress = Clampf((g_statCountUp - 0.3f) * 3.0f, 0, 1);
    if (stat3Progress > 0) {
        snprintf(buf, sizeof(buf), "Highest Wave: %d", g_game.highestWave + 1);

        float offsetX = (1.0f - EaseOutQuad(stat3Progress)) * -100;
        int bw = (int)MeasureTextEx(statsFont, buf, 24, 1).x;
        Color statColor = {255, 255, 200, (unsigned char)(255 * stat3Progress)};

        if (stat3Progress > 0.5f) {
            LlzDrawGemShape(LLZ_SHAPE_STAR, g_screenWidth / 2 - bw / 2 - 25 + offsetX, statsY + 92, 8, LLZ_GEM_DIAMOND);
        }
        DrawTextEx(statsFont, buf, (Vector2){g_screenWidth / 2 - bw / 2 + offsetX, statsY + 80}, 24, 1, statColor);
    }

    // Victory gem display (row of gems)
    float gemProgress = Clampf((g_statCountUp - 0.5f) * 2.0f, 0, 1);
    if (gemProgress > 0.3f) {
        int gemY = statsY + 130;
        LlzGemColor gems[] = {LLZ_GEM_RUBY, LLZ_GEM_TOPAZ, LLZ_GEM_EMERALD, LLZ_GEM_SAPPHIRE, LLZ_GEM_AMETHYST};
        for (int i = 0; i < 5; i++) {
            float delay = i * 0.1f;
            float gemAlpha = Clampf((gemProgress - 0.3f - delay) * 4.0f, 0, 1);
            if (gemAlpha > 0) {
                float bob = sinf(g_game.bgTime * 3.0f + i * 0.8f) * 3.0f;
                float gemX = g_screenWidth / 2 - 80 + i * 40;
                LlzDrawGemShape(LLZ_SHAPE_DIAMOND, gemX, gemY + bob, 12 * gemAlpha, gems[i]);
            }
        }
    }

    // Continue prompt (fades in last)
    float promptProgress = Clampf((g_statCountUp - 0.7f) * 2.5f, 0, 1);
    if (promptProgress > 0) {
        Font promptFont = LlzFontGet(LLZ_FONT_UI, 18);
        const char *prompt = "Press any button to return to menu";
        int pw = (int)MeasureTextEx(promptFont, prompt, 18, 1).x;

        float pulse = 0.6f + 0.4f * sinf(g_game.bgTime * 3.0f);
        Color promptColor = {255, 215, 0, (unsigned char)(200 * promptProgress * pulse)};

        DrawTextEx(promptFont, prompt, (Vector2){g_screenWidth / 2 - pw / 2, g_screenHeight - 50}, 18, 1, promptColor);
    }
}

static void DrawBackground(void) {
    DrawRectangle(0, 0, g_screenWidth, g_screenHeight, COLOR_BG);

    int gridSize = 40;
    float offsetX = fmodf(g_game.camera.pos.x, gridSize);
    float offsetY = fmodf(g_game.camera.pos.y, gridSize);

    for (int x = -gridSize; x <= g_screenWidth + gridSize; x += gridSize)
        DrawLine(x - (int)offsetX, 0, x - (int)offsetX, g_screenHeight, COLOR_BG_GRID);
    for (int y = -gridSize; y <= g_screenHeight + gridSize; y += gridSize)
        DrawLine(0, y - (int)offsetY, g_screenWidth, y - (int)offsetY, COLOR_BG_GRID);

    Vector2 tl = WorldToScreen((Vector2){WORLD_PADDING, WORLD_PADDING});
    Vector2 br = WorldToScreen((Vector2){WORLD_WIDTH - WORLD_PADDING, WORLD_HEIGHT - WORLD_PADDING});

    if (tl.y >= 0 && tl.y <= g_screenHeight)
        DrawLineEx((Vector2){fmaxf(0, tl.x), tl.y}, (Vector2){fminf(g_screenWidth, br.x), tl.y}, 3, COLOR_WORLD_BORDER);
    if (br.y >= 0 && br.y <= g_screenHeight)
        DrawLineEx((Vector2){fmaxf(0, tl.x), br.y}, (Vector2){fminf(g_screenWidth, br.x), br.y}, 3, COLOR_WORLD_BORDER);
    if (tl.x >= 0 && tl.x <= g_screenWidth)
        DrawLineEx((Vector2){tl.x, fmaxf(0, tl.y)}, (Vector2){tl.x, fminf(g_screenHeight, br.y)}, 3, COLOR_WORLD_BORDER);
    if (br.x >= 0 && br.x <= g_screenWidth)
        DrawLineEx((Vector2){br.x, fmaxf(0, tl.y)}, (Vector2){br.x, fminf(g_screenHeight, br.y)}, 3, COLOR_WORLD_BORDER);
}

// =============================================================================
// INPUT HANDLERS
// =============================================================================

static void HandleMenuInput(const LlzInputState *input) {
    // Keyboard/scroll navigation
    if (input->scrollDelta > 0.5f || input->downPressed) g_game.menuIndex = (g_game.menuIndex + 1) % 2;
    else if (input->scrollDelta < -0.5f || input->upPressed) g_game.menuIndex = (g_game.menuIndex + 1) % 2;

    // Mouse hover detection for menu buttons
    // Buttons are at baseY=220 with 55px spacing, each roughly 50px tall
    int baseY = 220;
    int buttonHeight = 50;
    Vector2 mouse = input->mousePos;
    for (int i = 0; i < 2; i++) {
        int btnTop = baseY + i * 55 - 10;
        int btnBot = btnTop + buttonHeight;
        if (mouse.y >= btnTop && mouse.y <= btnBot &&
            mouse.x >= g_screenWidth / 2 - 100 && mouse.x <= g_screenWidth / 2 + 100) {
            g_game.menuIndex = i;
        }
    }

    if (input->selectPressed || input->tap) {
        if (g_game.menuIndex == 0) {
            // Go to class select first (before weapon select)
            g_game.state = GAME_STATE_CLASS_SELECT;
            g_classSelectEntrance = 0;  // Reset class select entrance animation
            g_classCarouselPos = (float)g_game.classSelectIndex;  // Start at current selection
            g_classCarouselTarget = g_classCarouselPos;
            memset(g_classCardGlow, 0, sizeof(g_classCardGlow));
        }
        else g_wantsClose = true;
    }
    if (input->backReleased) g_wantsClose = true;
}

static void HandleClassSelectInput(const LlzInputState *input) {
    int numClasses = CLASS_COUNT;
    if (input->scrollDelta > 0.5f || input->downPressed)
        g_game.classSelectIndex = (g_game.classSelectIndex + 1) % numClasses;
    else if (input->scrollDelta < -0.5f || input->upPressed)
        g_game.classSelectIndex = (g_game.classSelectIndex - 1 + numClasses) % numClasses;

    if (input->selectPressed || input->tap) {
        // Confirm class selection and go to weapon select
        g_game.selectedClass = (PlayerClass)g_game.classSelectIndex;
        g_game.state = GAME_STATE_WEAPON_SELECT;
        g_weaponSelectEntrance = 0;  // Reset weapon select entrance animation
        g_weaponCarouselPos = (float)g_game.weaponSelectIndex;  // Start at current selection
        g_weaponCarouselTarget = g_weaponCarouselPos;
        memset(g_weaponCardGlow, 0, sizeof(g_weaponCardGlow));
    }
    if (input->backReleased) {
        g_game.state = GAME_STATE_MENU;
        g_menuEntranceTime = 0;  // Reset menu entrance animation
    }
}

static void HandleWeaponSelectInput(const LlzInputState *input) {
    int numStarting = STARTING_WEAPON_COUNT;
    if (input->scrollDelta > 0.5f || input->downPressed)
        g_game.weaponSelectIndex = (g_game.weaponSelectIndex + 1) % numStarting;
    else if (input->scrollDelta < -0.5f || input->upPressed)
        g_game.weaponSelectIndex = (g_game.weaponSelectIndex - 1 + numStarting) % numStarting;

    if (input->selectPressed || input->tap) {
        g_game.startingWeapon = (WeaponType)g_game.weaponSelectIndex;
        GameReset();
        g_game.state = GAME_STATE_PLAYING;
    }
    if (input->backReleased) {
        // Go back to class select instead of menu
        g_game.state = GAME_STATE_CLASS_SELECT;
        g_classSelectEntrance = 0;  // Reset class select entrance animation
        g_classCarouselPos = (float)g_game.classSelectIndex;
        g_classCarouselTarget = g_classCarouselPos;
        memset(g_classCardGlow, 0, sizeof(g_classCardGlow));
    }
}

static void HandleLevelUpInput(const LlzInputState *input) {
    int totalChoices = NUM_UPGRADE_CHOICES + 1;

    // Horizontal scroll for carousel navigation
    if (input->scrollDelta > 0.5f) {
        if (g_game.selectedUpgrade < totalChoices - 1) {
            g_game.selectedUpgrade++;
            g_game.targetOffset = 0;  // Reset smooth animation
        }
    } else if (input->scrollDelta < -0.5f) {
        if (g_game.selectedUpgrade > 0) {
            g_game.selectedUpgrade--;
            g_game.targetOffset = 0;
        }
    }

    // Up: cycle through potion inventory
    if (input->upPressed) {
        g_game.selectedPotion = (g_game.selectedPotion - 1 + MAX_INVENTORY_POTIONS) % MAX_INVENTORY_POTIONS;
    }
    // Down: use selected potion
    if (input->downPressed) {
        if (g_game.inventory[g_game.selectedPotion].active) {
            ActivateBuff(g_game.inventory[g_game.selectedPotion].type);
            g_game.inventory[g_game.selectedPotion].active = false;
        }
    }

    // Select/tap to confirm upgrade choice
    if (input->selectPressed || input->tap) {
        ApplyUpgrade(g_game.selectedUpgrade);
    }
}

static void UseSelectedPotion(void) {
    if (g_game.inventory[g_game.selectedPotion].active) {
        PotionType type = g_game.inventory[g_game.selectedPotion].type;
        ActivateBuff(type);
        g_game.inventory[g_game.selectedPotion].active = false;

        // Spawn feedback text
        char msg[32];
        snprintf(msg, sizeof(msg), "%s!", GetPotionName(type));
        SpawnTextPopup(g_game.player.pos, msg, GetPotionColor(type), 1.2f);
    }
}

static void HandlePlayInput(const LlzInputState *input) {
    if (input->backReleased) g_game.state = GAME_STATE_PAUSED;

    // Potion controls: Up to cycle selection, Down to use
    if (input->upPressed) {
        // Cycle through inventory slots
        int startSlot = g_game.selectedPotion;
        do {
            g_game.selectedPotion = (g_game.selectedPotion + 1) % MAX_INVENTORY_POTIONS;
        } while (!g_game.inventory[g_game.selectedPotion].active &&
                 g_game.selectedPotion != startSlot);
    }

    if (input->downPressed) {
        UseSelectedPotion();
    }
}

static void HandlePausedInput(const LlzInputState *input) {
    if (input->selectPressed || input->tap) g_game.state = GAME_STATE_PLAYING;
    if (input->backReleased) {
        g_game.state = GAME_STATE_MENU;
        g_menuEntranceTime = 0;  // Reset menu entrance animation
    }
}

static void HandleGameOverInput(const LlzInputState *input) {
    if (input->selectPressed || input->tap || input->backReleased) {
        g_game.state = GAME_STATE_MENU;
        g_menuEntranceTime = 0;  // Reset menu entrance animation
    }
}

static void HandleVictoryInput(const LlzInputState *input) {
    if (input->selectPressed || input->tap || input->backReleased) {
        g_game.state = GAME_STATE_MENU;
        g_menuEntranceTime = 0;  // Reset menu entrance animation
    }
}

// =============================================================================
// PUBLIC API
// =============================================================================

void GameInit(int width, int height) {
    g_screenWidth = width;
    g_screenHeight = height;
    g_wantsClose = false;

    g_font = LlzFontGet(LLZ_FONT_UI, 32);
    if (g_font.texture.id == 0) g_font = GetFontDefault();

    memset(&g_game, 0, sizeof(g_game));
    g_game.state = GAME_STATE_MENU;
    g_game.startingWeapon = WEAPON_DISTANCE;
    g_game.selectedClass = CLASS_BALANCED;  // Default class
    g_game.classSelectIndex = 0;

    // Initialize SDK animated background
    LlzBackgroundInit(width, height);
    LlzBackgroundSetStyle(LLZ_BG_STYLE_CONSTELLATION, false);  // Nice starfield for menu
    LlzBackgroundSetColors((Color){30, 50, 80, 255}, (Color){0, 200, 200, 255});  // Cyan theme
    g_bgSystemInitialized = true;

    // Reset menu animation state
    g_menuTitleGlow = 0.0f;
    g_menuEntranceTime = 0.0f;
    g_menuButtonScale[0] = g_menuButtonScale[1] = 1.0f;
    // Class select animation
    g_classSelectEntrance = 0.0f;
    g_classCarouselPos = 0.0f;
    g_classCarouselTarget = 0.0f;
    memset(g_classCardGlow, 0, sizeof(g_classCardGlow));
    // Weapon select animation
    g_weaponSelectEntrance = 0.0f;
    g_weaponCarouselPos = 0.0f;
    g_weaponCarouselTarget = 0.0f;
    memset(g_weaponCardGlow, 0, sizeof(g_weaponCardGlow));
    g_gameOverEntrance = 0.0f;
    g_statCountUp = 0.0f;

    printf("[LLZSURVIVORS] Initialized %dx%d, World: %dx%d\n", width, height, WORLD_WIDTH, WORLD_HEIGHT);
}

void GameReset(void) {
    // Get class stats for the selected class
    const ClassStats *cls = &CLASS_STATS[g_game.selectedClass];

    Player *p = &g_game.player;
    *p = (Player){
        .pos = {WORLD_WIDTH / 2.0f, WORLD_HEIGHT / 2.0f},
        .angle = -PI / 2,
        .speed = PLAYER_SPEED * cls->speedMultiplier,
        .baseSpeed = PLAYER_SPEED * cls->speedMultiplier,
        .isMoving = true,
        .hp = cls->baseHP,
        .maxHp = cls->baseHP,
        .level = 1, .xp = 0, .xpToNextLevel = XP_THRESHOLDS[0],
        .magnetRange = PLAYER_BASE_XP_MAGNET_RANGE,
        .healthRegen = PLAYER_BASE_REGEN_RATE,
        .damageMultiplier = 1.0f,
        .stationaryTime = 0,
        // Class-specific stats
        .playerClass = g_game.selectedClass,
        .classWeaponBonus = cls->weaponDamageBonus,
        .xpMultiplier = cls->xpMultiplier,
        // New stats
        .attackSpeedMult = 1.0f,
        .critChance = 0,
        .areaMultiplier = 1.0f,
        .bonusProjectiles = 0,
        .armor = cls->armorPercent,  // Starting armor from class
        .lifesteal = 0,
        .dodgeChance = 0,
        .thorns = 0,
        .upgradePoints = 0
    };

    g_game.camera = (GameCamera){.pos = p->pos, .target = p->pos};

    memset(g_game.weapons, 0, sizeof(g_game.weapons));
    g_game.weapons[g_game.startingWeapon].tier = 1;
    g_game.weapons[g_game.startingWeapon].cooldownTimer = 0;

    memset(g_game.projectiles, 0, sizeof(g_game.projectiles));
    memset(g_game.orbitOrbs, 0, sizeof(g_game.orbitOrbs));
    memset(&g_game.wave, 0, sizeof(g_game.wave));
    memset(&g_game.melee, 0, sizeof(g_game.melee));
    memset(g_game.lightning, 0, sizeof(g_game.lightning));
    memset(g_game.seekers, 0, sizeof(g_game.seekers));
    memset(g_game.boomerangs, 0, sizeof(g_game.boomerangs));
    memset(g_game.poisonClouds, 0, sizeof(g_game.poisonClouds));
    memset(g_game.chains, 0, sizeof(g_game.chains));
    memset(g_game.enemies, 0, sizeof(g_game.enemies));
    memset(g_game.enemyBullets, 0, sizeof(g_game.enemyBullets));
    memset(g_game.mines, 0, sizeof(g_game.mines));
    memset(g_game.dangerZones, 0, sizeof(g_game.dangerZones));
    g_game.dangerZoneSpawnTimer = DANGER_ZONE_SPAWN_INTERVAL;
    memset(g_game.xpGems, 0, sizeof(g_game.xpGems));
    memset(g_game.potions, 0, sizeof(g_game.potions));
    memset(g_game.inventory, 0, sizeof(g_game.inventory));
    memset(g_game.buffs, 0, sizeof(g_game.buffs));
    memset(g_game.particles, 0, sizeof(g_game.particles));
    memset(g_game.popups, 0, sizeof(g_game.popups));
    memset(g_game.uiParticles, 0, sizeof(g_game.uiParticles));

    g_game.xpCombo = 0;
    g_game.comboTimer = 0;
    g_game.screenFlash = 0;
    g_game.xpBarPulse = 0;

    g_game.spawner = (SpawnSystem){.spawnTimer = 1.0f, .spawnInterval = 1.5f};
    g_game.gameTime = 0;
    g_game.killCount = 0;
    g_game.highestWave = 0;
    g_game.screenShake = 0;
    g_game.selectedPotion = 0;

    // Reset juice effect state
    g_hitstopTimer = 0;
    g_levelUpFreeze = 0;
    g_levelUpCelebration = 0;
    g_damageVignette = 0;
    g_lastHitWasCrit = false;

    // Reset Phase 2 effect state
    g_killStreak = 0;
    g_killStreakTimer = 0;
    g_killStreakDisplay = 0;
    g_killStreakMilestone = 0;
    g_lastWave = 0;
    g_waveCelebration = 0;
    memset(g_dyingEnemies, 0, sizeof(g_dyingEnemies));
    memset(g_spawnWarnings, 0, sizeof(g_spawnWarnings));

    // Reset kill combo system
    g_game.killCombo = 0;
    g_game.killComboTimer = 0;
    g_game.comboTier = COMBO_NONE;
    g_game.prevComboTier = COMBO_NONE;
    g_game.comboTierFlash = 0;
    g_game.highestCombo = 0;

    // Reset enemy pool (Walker always unlocked, others start locked)
    memset(g_enemyPoolUnlocked, 0, sizeof(g_enemyPoolUnlocked));
    g_enemyPoolUnlocked[ENEMY_WALKER] = true;
    g_enemyIntroTimer = 0;
    g_enemyIntroActive = false;

    // Initialize kill milestone rewards
    InitMilestones();
}

void GameUpdate(const LlzInputState *input, float dt) {
    g_game.bgTime += dt;

    // Update SDK animated background
    if (g_bgSystemInitialized) {
        LlzBackgroundUpdate(dt);
    }

    // Update Phase 3 menu animation timers
    g_menuTitleGlow += dt;
    g_lowHpPulse += dt;

    // Menu entrance animation
    if (g_game.state == GAME_STATE_MENU && g_menuEntranceTime < 1.0f) {
        g_menuEntranceTime += dt * 2.0f;
        if (g_menuEntranceTime > 1.0f) g_menuEntranceTime = 1.0f;
    }

    // Class select entrance animation
    if (g_game.state == GAME_STATE_CLASS_SELECT && g_classSelectEntrance < 1.0f) {
        g_classSelectEntrance += dt * 2.5f;
        if (g_classSelectEntrance > 1.0f) g_classSelectEntrance = 1.0f;
    }

    // Update class carousel animation (smooth carousel)
    if (g_game.state == GAME_STATE_CLASS_SELECT) {
        g_classCarouselTarget = (float)g_game.classSelectIndex;
        float diff = g_classCarouselTarget - g_classCarouselPos;
        g_classCarouselPos += diff * 10.0f * dt;  // Smooth ease-out
        if (fabsf(diff) < 0.01f) g_classCarouselPos = g_classCarouselTarget;

        // Update glow intensity for each class card
        for (int i = 0; i < CLASS_COUNT; i++) {
            float targetGlow = (i == g_game.classSelectIndex) ? 1.0f : 0.0f;
            g_classCardGlow[i] += (targetGlow - g_classCardGlow[i]) * 8.0f * dt;
        }
    }

    // Weapon select entrance animation
    if (g_game.state == GAME_STATE_WEAPON_SELECT && g_weaponSelectEntrance < 1.0f) {
        g_weaponSelectEntrance += dt * 2.5f;
        if (g_weaponSelectEntrance > 1.0f) g_weaponSelectEntrance = 1.0f;
    }

    // Update weapon carousel animation (smooth carousel like Bejeweled)
    if (g_game.state == GAME_STATE_WEAPON_SELECT) {
        g_weaponCarouselTarget = (float)g_game.weaponSelectIndex;
        float diff = g_weaponCarouselTarget - g_weaponCarouselPos;
        g_weaponCarouselPos += diff * 10.0f * dt;  // Smooth ease-out
        if (fabsf(diff) < 0.01f) g_weaponCarouselPos = g_weaponCarouselTarget;

        // Update glow intensity for each weapon card
        for (int i = 0; i < STARTING_WEAPON_COUNT; i++) {
            float targetGlow = (i == g_game.weaponSelectIndex) ? 1.0f : 0.0f;
            g_weaponCardGlow[i] += (targetGlow - g_weaponCardGlow[i]) * 8.0f * dt;
        }
    }

    // Game over / Victory entrance and stat count-up animation
    if (g_game.state == GAME_STATE_GAME_OVER || g_game.state == GAME_STATE_VICTORY) {
        if (g_gameOverEntrance < 1.0f) {
            g_gameOverEntrance += dt * 2.5f;
            if (g_gameOverEntrance > 1.0f) g_gameOverEntrance = 1.0f;
        }
        if (g_gameOverEntrance > 0.3f && g_statCountUp < 1.0f) {
            g_statCountUp += dt * 1.5f;
            if (g_statCountUp > 1.0f) g_statCountUp = 1.0f;
            // Animate displayed values
            g_displayedKills = (int)(g_game.killCount * g_statCountUp);
            g_displayedTime = g_game.gameTime * g_statCountUp;
        }
    }

    // Update button hover scales for menu
    for (int i = 0; i < 2; i++) {
        float targetScale = (i == g_game.menuIndex) ? 1.15f : 1.0f;
        g_menuButtonScale[i] += (targetScale - g_menuButtonScale[i]) * dt * 10.0f;
    }

    // Update HP flash (triggered when HP decreases)
    if (g_hpFlash > 0) {
        g_hpFlash -= dt * 4.0f;
        if (g_hpFlash < 0) g_hpFlash = 0;
    }

    // Update danger glow (fade out if no enemies nearby)
    for (int i = 0; i < 4; i++) {
        g_dangerGlow[i] *= (1.0f - dt * 3.0f);
        if (g_dangerGlow[i] < 0.01f) g_dangerGlow[i] = 0;
    }

    // Update juice effect timers (always update, independent of game state)
    if (g_hitstopTimer > 0) {
        g_hitstopTimer -= dt;
        if (g_hitstopTimer < 0) g_hitstopTimer = 0;
    }
    if (g_levelUpFreeze > 0) {
        g_levelUpFreeze -= dt;
        if (g_levelUpFreeze < 0) g_levelUpFreeze = 0;
    }
    if (g_levelUpCelebration > 0) {
        g_levelUpCelebration -= dt * 2.0f;
        if (g_levelUpCelebration < 0) g_levelUpCelebration = 0;
    }
    if (g_damageVignette > 0) {
        g_damageVignette -= dt * VIGNETTE_FADE_SPEED;
        if (g_damageVignette < 0) g_damageVignette = 0;
    }

    // Update graze system timers
    if (g_game.grazeFlash > 0) {
        g_game.grazeFlash -= dt * 4.0f;
        if (g_game.grazeFlash < 0) g_game.grazeFlash = 0;
    }
    if (g_game.grazeComboTimer > 0) {
        g_game.grazeComboTimer -= dt;
        if (g_game.grazeComboTimer <= 0) {
            g_game.grazeCombo = 0;  // Reset combo when timer expires
        }
    }

    // Update kill streak timer
    if (g_killStreakTimer > 0) {
        g_killStreakTimer -= dt;
        if (g_killStreakTimer <= 0) {
            g_killStreak = 0;  // Reset streak
        }
    }
    if (g_killStreakDisplay > 0) {
        g_killStreakDisplay -= dt;
    }

    if (g_game.screenShake > 0) {
        g_game.screenShake -= dt * 5.0f;
        if (g_game.screenShake < 0) g_game.screenShake = 0;
        g_game.screenShakeX = sinf(g_game.bgTime * 50) * g_game.screenShake * 8;
        g_game.screenShakeY = cosf(g_game.bgTime * 60) * g_game.screenShake * 6;
    }

    // Update screen flash
    if (g_game.screenFlash > 0) {
        g_game.screenFlash -= dt * 3.0f;
        if (g_game.screenFlash < 0) g_game.screenFlash = 0;
    }

    // Update combo timer
    if (g_game.comboTimer > 0) {
        g_game.comboTimer -= dt;
        if (g_game.comboTimer <= 0) g_game.xpCombo = 0;
    }

    // Update XP bar pulse
    if (g_game.xpBarPulse > 0) {
        g_game.xpBarPulse -= dt * 5.0f;
        if (g_game.xpBarPulse < 0) g_game.xpBarPulse = 0;
    }

    // Always update UI effects
    UpdateTextPopups(dt);
    UpdateUIParticles(dt);

    // Update Phase 2 effects (always, even when frozen)
    UpdateDyingEnemies(dt);
    UpdateSpawnWarnings(dt);
    UpdateKillStreak(dt);
    UpdateMilestones(dt);  // Update milestone celebration timers
    if (g_waveCelebration > 0) {
        g_waveCelebration -= dt;
        if (g_waveCelebration < 0) g_waveCelebration = 0;
    }

    // Enemy introduction timer
    if (g_enemyIntroTimer > 0) {
        g_enemyIntroTimer -= dt;
        if (g_enemyIntroTimer <= 0) {
            g_enemyIntroTimer = 0;
            g_enemyIntroActive = false;
        }
    }

    // Check for hitstop/freeze (skip game logic, but allow particles and effects)
    bool frozen = g_hitstopTimer > 0 || g_levelUpFreeze > 0;

    switch (g_game.state) {
        case GAME_STATE_MENU: HandleMenuInput(input); break;
        case GAME_STATE_CLASS_SELECT: HandleClassSelectInput(input); break;
        case GAME_STATE_WEAPON_SELECT: HandleWeaponSelectInput(input); break;
        case GAME_STATE_PLAYING:
            HandlePlayInput(input);
            if (g_game.state != GAME_STATE_PLAYING) break;

            // Skip game updates during hitstop/freeze, but always update particles
            UpdateParticles(dt);
            if (frozen) break;

            g_game.gameTime += dt;
            UpdatePlayer(input, dt);
            UpdateGameCamera(dt);
            PopulateSpatialGrid();  // Build collision grid for this frame
            UpdateWeapons(dt);
            UpdateSpawner(dt);
            UpdateDangerZones(dt);
            UpdateEnemies(dt);
            UpdateEnemyBullets(dt);
            UpdateMines(dt);
            UpdateXPGems(dt);
            UpdatePotions(dt);
            UpdateBuffs(dt);
            break;
        case GAME_STATE_LEVEL_UP: HandleLevelUpInput(input); break;
        case GAME_STATE_PAUSED: HandlePausedInput(input); break;
        case GAME_STATE_GAME_OVER: HandleGameOverInput(input); break;
        case GAME_STATE_VICTORY: HandleVictoryInput(input); break;
    }
}

void GameDraw(void) {
    bool shaking = g_game.screenShake > 0;
    if (shaking) { rlPushMatrix(); rlTranslatef(g_game.screenShakeX, g_game.screenShakeY, 0); }

    switch (g_game.state) {
        case GAME_STATE_MENU: DrawMenu(); break;
        case GAME_STATE_CLASS_SELECT: DrawClassSelect(); break;
        case GAME_STATE_WEAPON_SELECT: DrawWeaponSelect(); break;
        default:
            DrawBackground();
            DrawDangerZones();   // Draw danger zones under entities
            DrawPoisonClouds();  // Draw under everything
            DrawXPGems();
            DrawPotions();
            DrawEnemies();
            DrawHornetLasers();  // Laser beams on top of enemies
            DrawEnemyBullets(); // Bullet hell patterns
            DrawMines();        // Bomber mines
            DrawDyingEnemies();  // Death animations over enemies
            DrawProjectiles();
            DrawSeekers();
            DrawBoomerangs();
            DrawOrbit();
            DrawWave();
            DrawMelee();
            DrawLightning();
            DrawChainLightning();
            DrawPlayer();
            DrawParticles();
            DrawTextPopups();
            DrawHUD();
            DrawSpawnWarnings();  // Edge indicators for incoming enemies
            DrawDangerGlow();     // Red glow on edges for nearby off-screen enemies
            DrawUIParticles();  // Draw over HUD

            // Announcements
            DrawWaveCelebration();
            DrawKillStreakAnnouncement();
            DrawComboTierAnnouncement();  // Combo tier change announcement
            DrawMilestoneCelebration();  // Milestone reward announcement
            DrawEnemyIntroduction();

            // Combo meter HUD element
            DrawComboMeter();

            // Screen flash overlay
            if (g_game.screenFlash > 0) {
                Color flashColor = g_game.screenFlashColor;
                flashColor.a = (unsigned char)(80 * g_game.screenFlash);
                DrawRectangle(0, 0, g_screenWidth, g_screenHeight, flashColor);
            }

            // Damage vignette (red edges when hurt)
            if (g_damageVignette > 0) {
                int vignetteWidth = 80;
                unsigned char alpha = (unsigned char)(120 * g_damageVignette);
                Color vignetteOuter = {180, 0, 0, alpha};
                Color vignetteInner = {180, 0, 0, 0};

                // Left edge
                DrawRectangleGradientH(0, 0, vignetteWidth, g_screenHeight, vignetteOuter, vignetteInner);
                // Right edge
                DrawRectangleGradientH(g_screenWidth - vignetteWidth, 0, vignetteWidth, g_screenHeight, vignetteInner, vignetteOuter);
                // Top edge
                DrawRectangleGradientV(0, 0, g_screenWidth, vignetteWidth, vignetteOuter, vignetteInner);
                // Bottom edge
                DrawRectangleGradientV(0, g_screenHeight - vignetteWidth, g_screenWidth, vignetteWidth, vignetteInner, vignetteOuter);

                // Corner overlays for more intense effect
                unsigned char cornerAlpha = (unsigned char)(80 * g_damageVignette);
                Color cornerOuter = {180, 0, 0, cornerAlpha};
                DrawRectangleGradientEx((Rectangle){0, 0, vignetteWidth, vignetteWidth}, cornerOuter, vignetteInner, vignetteInner, vignetteInner);
                DrawRectangleGradientEx((Rectangle){g_screenWidth - vignetteWidth, 0, vignetteWidth, vignetteWidth}, vignetteInner, cornerOuter, vignetteInner, vignetteInner);
                DrawRectangleGradientEx((Rectangle){0, g_screenHeight - vignetteWidth, vignetteWidth, vignetteWidth}, vignetteInner, vignetteInner, vignetteInner, cornerOuter);
                DrawRectangleGradientEx((Rectangle){g_screenWidth - vignetteWidth, g_screenHeight - vignetteWidth, vignetteWidth, vignetteWidth}, vignetteInner, vignetteInner, cornerOuter, vignetteInner);
            }

            if (g_game.state == GAME_STATE_LEVEL_UP) {
                DrawLevelUpScreen();
            }

            if (g_game.state == GAME_STATE_PAUSED) {
                DrawRectangle(0, 0, g_screenWidth, g_screenHeight, (Color){0, 0, 0, 150});
                const char *text = "PAUSED";
                DrawTextEx(g_font, text, (Vector2){g_screenWidth / 2 - (int)MeasureTextEx(g_font, text, 48, 1).x / 2, g_screenHeight / 2 - 24}, 48, 1, COLOR_TEXT);
                DrawTextEx(g_font, "Select: Resume | Back: Menu", (Vector2){g_screenWidth / 2 - 100, g_screenHeight / 2 + 40}, 18, 1, COLOR_TEXT_DIM);
            }
            if (g_game.state == GAME_STATE_GAME_OVER) DrawGameOver();
            if (g_game.state == GAME_STATE_VICTORY) DrawVictory();
            break;
    }

    if (shaking) rlPopMatrix();
}

void GameShutdown(void) {
    // Shutdown SDK background
    if (g_bgSystemInitialized) {
        LlzBackgroundShutdown();
        g_bgSystemInitialized = false;
    }

    g_wantsClose = false;
    printf("[LLZSURVIVORS] Shutdown\n");
}

bool GameWantsClose(void) {
    return g_wantsClose;
}
