// LLZ Survivors - Vampire Survivors/Brotato-lite Game Plugin
// Header file with types, constants, and API declarations

#ifndef LLZSURVIVORS_GAME_H
#define LLZSURVIVORS_GAME_H

#include <stdbool.h>
#include "raylib.h"
#include "llz_sdk_input.h"

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// CONSTANTS
// =============================================================================

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 480

// World/Arena size (much larger than screen, camera follows player)
#define WORLD_WIDTH 3000
#define WORLD_HEIGHT 2000
#define WORLD_PADDING 40

// Minimap
#define MINIMAP_WIDTH 120
#define MINIMAP_HEIGHT 80
#define MINIMAP_X (SCREEN_WIDTH - MINIMAP_WIDTH - 10)
#define MINIMAP_Y 10
#define MINIMAP_PLAYER_SIZE 4
#define MINIMAP_ENEMY_SIZE 2

// Player
#define PLAYER_SIZE 20.0f
#define PLAYER_SPEED 80.0f
#define PLAYER_MAX_HP 100
#define PLAYER_INVINCIBILITY_TIME 1.0f
#define PLAYER_BASE_XP_MAGNET_RANGE 60.0f
#define PLAYER_PICKUP_RANGE 25.0f
#define PLAYER_BASE_REGEN_RATE 0.0f  // HP per second when stationary

// Weapons - Base stats (upgrade tiers multiply these)
#define MAX_PROJECTILES 128
#define MAX_ORBIT_ORBS 8
#define MAX_LIGHTNING_STRIKES 16

// Melee weapon
#define MELEE_BASE_DAMAGE 15
#define MELEE_BASE_RANGE 50.0f
#define MELEE_BASE_ARC 90.0f  // degrees
#define MELEE_BASE_COOLDOWN 0.6f

// Distance weapon (bullets)
#define BULLET_SPEED 400.0f
#define BULLET_BASE_DAMAGE 8
#define BULLET_SIZE 6.0f
#define BULLET_BASE_COOLDOWN 0.5f

// Magic weapon (wave)
#define WAVE_BASE_DAMAGE 6
#define WAVE_BASE_COOLDOWN 3.0f
#define WAVE_BASE_RADIUS 80.0f
#define WAVE_DURATION 0.5f

// Radius weapon (orbit)
#define ORBIT_BASE_DAMAGE 12
#define ORBIT_SIZE 10.0f
#define ORBIT_BASE_RADIUS 50.0f
#define ORBIT_SPEED 3.0f
#define ORBIT_BASE_COUNT 2

// Mystic weapon (lightning)
#define LIGHTNING_BASE_DAMAGE 20
#define LIGHTNING_BASE_COOLDOWN 2.5f
#define LIGHTNING_BASE_STRIKES 1
#define LIGHTNING_RANGE 300.0f

// NEW WEAPONS

// Seeker weapon (homing missiles)
#define MAX_SEEKERS 16
#define SEEKER_BASE_DAMAGE 10
#define SEEKER_BASE_COOLDOWN 1.2f
#define SEEKER_SPEED 200.0f
#define SEEKER_TURN_RATE 3.0f
#define SEEKER_RANGE 400.0f
#define SEEKER_EXPLOSION_RADIUS 30.0f

// Boomerang weapon
#define MAX_BOOMERANGS 4
#define BOOMERANG_BASE_DAMAGE 14
#define BOOMERANG_BASE_COOLDOWN 2.0f
#define BOOMERANG_SPEED 280.0f
#define BOOMERANG_RANGE 250.0f
#define BOOMERANG_SIZE 16.0f
#define BOOMERANG_SPIN_SPEED 12.0f

// Poison weapon (toxic clouds)
#define MAX_POISON_CLOUDS 8
#define POISON_BASE_DAMAGE 3
#define POISON_TICK_RATE 0.5f
#define POISON_BASE_COOLDOWN 3.5f
#define POISON_DURATION 4.0f
#define POISON_RADIUS 50.0f
#define POISON_SLOW_PERCENT 30

// Chain weapon (chain lightning)
#define MAX_CHAINS 4
#define CHAIN_BASE_DAMAGE 18
#define CHAIN_BASE_COOLDOWN 2.0f
#define CHAIN_BASE_JUMPS 3
#define CHAIN_RANGE 200.0f
#define CHAIN_JUMP_RANGE 100.0f
#define CHAIN_DECAY 0.8f

// Enemies (base HP is 1, scales with time)
#define MAX_ENEMIES 128
#define ENEMY_SPAWN_MARGIN 100.0f
#define WALKER_SIZE 18.0f
#define WALKER_SPEED 50.0f
#define WALKER_BASE_HP 1
#define WALKER_DAMAGE 10
#define WALKER_XP 10
#define FAST_SIZE 14.0f
#define FAST_SPEED 90.0f
#define FAST_BASE_HP 1
#define FAST_DAMAGE 5
#define FAST_XP 8
#define TANK_SIZE 28.0f
#define TANK_SPEED 30.0f
#define TANK_BASE_HP 1
#define TANK_DAMAGE 20
#define TANK_XP 25

// New enemy types (Wave 5+)
#define SWARM_SIZE 10.0f
#define SWARM_SPEED 75.0f
#define SWARM_BASE_HP 1
#define SWARM_DAMAGE 3
#define SWARM_XP 5
#define SWARM_SPAWN_COUNT 5  // Spawns in groups

#define ELITE_SIZE 22.0f
#define ELITE_SPEED 55.0f
#define ELITE_BASE_HP 3
#define ELITE_DAMAGE 15
#define ELITE_XP 30

#define BRUTE_SIZE 35.0f
#define BRUTE_SPEED 25.0f
#define BRUTE_BASE_HP 5
#define BRUTE_DAMAGE 30
#define BRUTE_XP 50

#define BOSS_SIZE 50.0f
#define BOSS_SPEED 20.0f
#define BOSS_BASE_HP 15
#define BOSS_DAMAGE 40
#define BOSS_XP 150

// Hornet - ranged laser enemy (Wave 8)
#define HORNET_SIZE 16.0f
#define HORNET_SPEED 60.0f
#define HORNET_BASE_HP 2
#define HORNET_DAMAGE 5           // Contact damage (low)
#define HORNET_XP 35
#define HORNET_ATTACK_RANGE 250.0f    // Stops at this distance to attack
#define HORNET_LASER_DAMAGE 25        // Laser is powerful!
#define HORNET_LASER_CHARGE_TIME 1.5f // Warning time before laser fires
#define HORNET_LASER_DURATION 0.8f    // How long laser stays active
#define HORNET_LASER_COOLDOWN 3.0f    // Time between laser attacks
#define HORNET_LASER_WIDTH 6.0f       // Width of active laser beam

// =============================================================================
// BULLET HELL ENEMIES (Wave 12+)
// =============================================================================

// Spinner - fires spiral bullet patterns, vulnerable when eye opens
#define SPINNER_SIZE 24.0f
#define SPINNER_SPEED 35.0f
#define SPINNER_BASE_HP 4
#define SPINNER_DAMAGE 15
#define SPINNER_XP 60
#define SPINNER_ATTACK_RANGE 200.0f
#define SPINNER_BULLET_DAMAGE 12
#define SPINNER_BULLET_SPEED 120.0f
#define SPINNER_BULLETS_PER_WAVE 8      // Bullets fired per rotation
#define SPINNER_FIRE_RATE 0.15f         // Time between bullets in barrage
#define SPINNER_BARRAGE_COUNT 16        // Total bullets per barrage
#define SPINNER_COOLDOWN 2.5f           // Rest between barrages (vulnerable!)
#define SPINNER_VULNERABLE_TIME 1.5f    // How long eye stays open

// Mirror - creates decoys, only real one takes damage
#define MIRROR_SIZE 20.0f
#define MIRROR_SPEED 45.0f
#define MIRROR_BASE_HP 3
#define MIRROR_DAMAGE 12
#define MIRROR_XP 55
#define MIRROR_DECOY_COUNT 2            // Number of fake copies
#define MIRROR_SPLIT_COOLDOWN 4.0f      // Time between splits
#define MIRROR_REVEAL_TIME 0.8f         // Brief reveal of real one
#define MIRROR_DECOY_DURATION 6.0f      // How long decoys last

// Shielder - rotating shield blocks damage from one direction
#define SHIELDER_SIZE 26.0f
#define SHIELDER_SPEED 40.0f
#define SHIELDER_BASE_HP 5
#define SHIELDER_DAMAGE 18
#define SHIELDER_XP 65
#define SHIELDER_SHIELD_ARC 120.0f      // Degrees of coverage
#define SHIELDER_ROTATE_SPEED 1.5f      // Radians per second
#define SHIELDER_CHARGE_SPEED 150.0f    // Speed during charge attack
#define SHIELDER_CHARGE_COOLDOWN 3.0f
#define SHIELDER_CHARGE_DURATION 0.8f

// Bomber - drops delayed explosive mines
#define BOMBER_SIZE 22.0f
#define BOMBER_SPEED 50.0f
#define BOMBER_BASE_HP 3
#define BOMBER_DAMAGE 10
#define BOMBER_XP 50
#define BOMBER_MINE_DAMAGE 20
#define BOMBER_MINE_RADIUS 60.0f
#define BOMBER_MINE_DELAY 2.0f          // Time before mine explodes
#define BOMBER_DROP_COOLDOWN 1.5f
#define BOMBER_MINES_PER_DROP 3
#define BOMBER_VULNERABLE_AFTER_DROP 1.0f  // Stunned after dropping

// Phaser - phases in/out, only damageable when visible
#define PHASER_SIZE 18.0f
#define PHASER_SPEED 70.0f
#define PHASER_BASE_HP 3
#define PHASER_DAMAGE 15
#define PHASER_XP 55
#define PHASER_BULLET_DAMAGE 10
#define PHASER_BULLET_SPEED 150.0f
#define PHASER_PHASE_DURATION 2.0f      // Time spent phased out
#define PHASER_VISIBLE_DURATION 1.5f    // Time spent visible (vulnerable)
#define PHASER_BULLETS_ON_APPEAR 6      // Burst when becoming visible
#define PHASER_TELEPORT_RANGE 150.0f    // Distance to teleport

// Enemy bullet system
#define MAX_ENEMY_BULLETS 64
#define ENEMY_BULLET_SIZE 5.0f

// Mine system
#define MAX_MINES 16

// HP scaling: HP = BASE_HP + (gameTime * HP_SCALE_RATE)
#define HP_SCALE_RATE 0.15f

// XP Gems
#define MAX_XP_GEMS 128
#define XP_GEM_SIZE 8.0f
#define XP_GEM_MAGNET_SPEED 350.0f

// XP Popups and UI effects
#define MAX_POPUPS 32
#define MAX_UI_PARTICLES 32

// Spatial Grid for collision optimization
#define GRID_CELL_SIZE 64
#define GRID_WIDTH ((WORLD_WIDTH / GRID_CELL_SIZE) + 1)
#define GRID_HEIGHT ((WORLD_HEIGHT / GRID_CELL_SIZE) + 1)
#define MAX_ENTITIES_PER_CELL 32

// Graze system (near-miss bonus)
#define GRAZE_DISTANCE_MULTIPLIER 2.0f  // How close for graze (1.5x collision radius)
#define GRAZE_XP_BONUS 5                // XP per graze
#define GRAZE_COOLDOWN 0.15f            // Cooldown per bullet to prevent spam

// Kill combo system
#define KILL_COMBO_TIMEOUT 2.5f         // Seconds until combo resets

// Danger Zones
#define MAX_DANGER_ZONES 3
#define DANGER_ZONE_DURATION 15.0f
#define DANGER_ZONE_WARNING 3.0f
#define DANGER_ZONE_SPAWN_INTERVAL 25.0f
#define DANGER_ZONE_MIN_PLAYER_DIST 200.0f
#define DANGER_ZONE_BASE_RADIUS 80.0f
#define DANGER_ZONE_DAMAGE_TICK 0.5f   // Damage tick interval for fire/electric
#define DANGER_ZONE_FIRE_DAMAGE 8      // Damage per tick for fire
#define DANGER_ZONE_ELECTRIC_DAMAGE 4  // Damage per tick for electric (but faster)
#define DANGER_ZONE_SLOW_AMOUNT 0.5f   // Speed multiplier in slow zone

// Potions
#define MAX_POTIONS 32
#define MAX_INVENTORY_POTIONS 5
#define POTION_DROP_CHANCE 8  // percent

// Particles
#define MAX_PARTICLES 96

// Upgrades - 15 total types, 5 shown per level-up (randomly selected)
#define TOTAL_UPGRADE_TYPES 15
#define NUM_UPGRADE_CHOICES 5      // How many random choices shown at level up
#define MAX_SKILL_TIER 5
#define SKILL_TREE_WEAPONS 5

// Carousel UI
#define CAROUSEL_CARD_WIDTH 220
#define CAROUSEL_CARD_HEIGHT 180
#define CAROUSEL_SPACING 20
#define CAROUSEL_Y 100

// =============================================================================
// COLORS (8-bit pixel art aesthetic)
// =============================================================================

#define COLOR_BG            (Color){12, 12, 18, 255}
#define COLOR_BG_GRID       (Color){20, 20, 30, 255}

// Player
#define COLOR_PLAYER        (Color){0, 220, 220, 255}
#define COLOR_PLAYER_ARROW  (Color){255, 255, 255, 255}
#define COLOR_PLAYER_HURT   (Color){255, 100, 100, 255}

// Enemies
#define COLOR_WALKER        (Color){220, 60, 60, 255}
#define COLOR_FAST          (Color){255, 220, 50, 255}
#define COLOR_TANK          (Color){160, 80, 200, 255}
#define COLOR_SWARM         (Color){150, 255, 150, 255}   // Green swarm
#define COLOR_ELITE         (Color){255, 100, 100, 255}   // Bright red elite
#define COLOR_HORNET        (Color){255, 200, 50, 255}    // Yellow/orange hornet
#define COLOR_HORNET_LASER  (Color){255, 100, 100, 255}   // Red laser
#define COLOR_BRUTE         (Color){100, 60, 40, 255}     // Brown brute
#define COLOR_BOSS          (Color){255, 50, 200, 255}    // Magenta boss
// Bullet hell enemies
#define COLOR_SPINNER       (Color){200, 100, 255, 255}   // Purple spinner
#define COLOR_SPINNER_EYE   (Color){255, 50, 50, 255}     // Red eye when vulnerable
#define COLOR_MIRROR        (Color){180, 220, 255, 255}   // Light blue mirror
#define COLOR_MIRROR_REAL   (Color){255, 200, 100, 255}   // Gold when revealed
#define COLOR_SHIELDER      (Color){80, 180, 80, 255}     // Green shielder
#define COLOR_SHIELD        (Color){100, 255, 200, 255}   // Cyan shield
#define COLOR_BOMBER        (Color){180, 80, 50, 255}     // Dark orange bomber
#define COLOR_MINE          (Color){255, 100, 50, 255}    // Orange mine
#define COLOR_PHASER        (Color){150, 100, 200, 180}   // Translucent purple phaser
#define COLOR_ENEMY_BULLET  (Color){255, 150, 150, 255}   // Pink enemy bullets
#define COLOR_ENEMY_EYE     (Color){255, 255, 255, 255}

// Weapons
#define COLOR_MELEE         (Color){255, 150, 100, 255}
#define COLOR_BULLET        (Color){255, 220, 100, 255}
#define COLOR_ORBIT         (Color){100, 180, 255, 255}
#define COLOR_WAVE          (Color){200, 100, 255, 180}
#define COLOR_LIGHTNING     (Color){255, 255, 100, 255}
#define COLOR_SEEKER        (Color){255, 120, 200, 255}
#define COLOR_BOOMERANG     (Color){180, 255, 120, 255}
#define COLOR_POISON        (Color){120, 220, 80, 140}
#define COLOR_CHAIN         (Color){140, 200, 255, 255}

// XP Gems
#define COLOR_XP_SMALL      (Color){80, 255, 80, 255}
#define COLOR_XP_MEDIUM     (Color){80, 180, 255, 255}
#define COLOR_XP_LARGE      (Color){255, 180, 80, 255}

// Potions
#define COLOR_POTION_DAMAGE (Color){255, 80, 80, 255}
#define COLOR_POTION_SPEED  (Color){80, 200, 255, 255}
#define COLOR_POTION_SHIELD (Color){255, 220, 80, 255}
#define COLOR_POTION_MAGNET (Color){80, 255, 180, 255}

// UI
#define COLOR_UI_BG         (Color){20, 20, 35, 220}
#define COLOR_HP_BAR        (Color){220, 60, 60, 255}
#define COLOR_HP_BG         (Color){60, 20, 20, 255}
#define COLOR_XP_BAR        (Color){80, 200, 255, 255}
#define COLOR_XP_BG         (Color){20, 50, 80, 255}
#define COLOR_TEXT          (Color){255, 255, 255, 255}
#define COLOR_TEXT_DIM      (Color){150, 150, 170, 255}
#define COLOR_UPGRADE_SEL   (Color){80, 200, 255, 255}
#define COLOR_SKIP          (Color){100, 100, 120, 255}
#define COLOR_LOCKED        (Color){80, 80, 90, 255}

// Particles
#define COLOR_PARTICLE_HIT  (Color){255, 200, 100, 255}
#define COLOR_PARTICLE_DIE  (Color){255, 100, 100, 255}
#define COLOR_PARTICLE_XP   (Color){100, 255, 100, 255}

// Minimap
#define COLOR_MINIMAP_BG    (Color){10, 10, 15, 200}
#define COLOR_MINIMAP_BORDER (Color){60, 60, 80, 255}
#define COLOR_MINIMAP_PLAYER (Color){0, 255, 255, 255}
#define COLOR_MINIMAP_ENEMY (Color){255, 80, 80, 255}
#define COLOR_MINIMAP_XP    (Color){80, 255, 80, 180}
#define COLOR_WORLD_BORDER  (Color){80, 40, 40, 255}

// Danger Zones
#define COLOR_DANGER_FIRE     (Color){255, 100, 50, 140}   // Orange/red fire
#define COLOR_DANGER_SLOW     (Color){50, 150, 255, 140}   // Blue ice/slow
#define COLOR_DANGER_ELECTRIC (Color){255, 255, 80, 140}   // Yellow electric
#define COLOR_DANGER_WARNING  (Color){255, 200, 100, 100}  // Pulsing warning

// =============================================================================
// ENUMERATIONS
// =============================================================================

typedef enum {
    GAME_STATE_MENU = 0,
    GAME_STATE_CLASS_SELECT,  // NEW: Class selection before weapon select
    GAME_STATE_WEAPON_SELECT,
    GAME_STATE_PLAYING,
    GAME_STATE_LEVEL_UP,
    GAME_STATE_PAUSED,
    GAME_STATE_GAME_OVER,
    GAME_STATE_VICTORY      // Win by reaching level 20!
} GameState;

// =============================================================================
// KILL COMBO TIER SYSTEM
// =============================================================================

typedef enum {
    COMBO_NONE = 0,      // < 5 kills
    COMBO_NICE,          // 5-14
    COMBO_GREAT,         // 15-29
    COMBO_AMAZING,       // 30-49
    COMBO_INCREDIBLE,    // 50-74
    COMBO_UNSTOPPABLE,   // 75-99
    COMBO_GODLIKE,       // 100+
    COMBO_TIER_COUNT
} ComboTier;

typedef struct {
    int minKills;
    const char *name;
    Color color;
    float xpBonus;      // 1.0x to 2.0x
    float damageBonus;  // 0% to 30%
} ComboTierInfo;

// Static combo tier definitions
static const ComboTierInfo COMBO_TIERS[COMBO_TIER_COUNT] = {
    {0,   "",            {255, 255, 255, 255}, 1.0f,  0.0f},   // COMBO_NONE
    {5,   "NICE!",       {135, 206, 250, 255}, 1.1f,  0.05f},  // COMBO_NICE (sky blue)
    {15,  "GREAT!",      {50, 205, 50, 255},   1.2f,  0.10f},  // COMBO_GREAT (lime green)
    {30,  "AMAZING!",    {255, 215, 0, 255},   1.35f, 0.15f},  // COMBO_AMAZING (gold)
    {50,  "INCREDIBLE!", {255, 165, 0, 255},   1.5f,  0.20f},  // COMBO_INCREDIBLE (orange)
    {75,  "UNSTOPPABLE!",{255, 50, 50, 255},   1.75f, 0.25f},  // COMBO_UNSTOPPABLE (red)
    {100, "GODLIKE!",    {255, 0, 255, 255},   2.0f,  0.30f},  // COMBO_GODLIKE (magenta)
};

// =============================================================================
// KILL MILESTONE REWARDS SYSTEM
// =============================================================================

typedef enum {
    MILESTONE_HEAL,           // +30 HP
    MILESTONE_UPGRADE_POINT,  // +1 upgrade point
    MILESTONE_DAMAGE_BUFF,    // +5% permanent damage
    MILESTONE_SPEED_BUFF,     // +3% permanent speed
    MILESTONE_MAGNET_PULSE,   // Vacuum all XP on screen
    MILESTONE_NUKE,           // Damage all enemies on screen
} MilestoneReward;

typedef struct {
    int killThreshold;
    MilestoneReward reward;
    const char *name;
    const char *description;
    bool claimed;
} KillMilestone;

#define MAX_MILESTONES 8

typedef enum {
    WEAPON_MELEE = 0,     // Close range swipe
    WEAPON_DISTANCE,      // Bullets
    WEAPON_MAGIC,         // Wave/AOE
    WEAPON_RADIUS,        // Orbiting orbs
    WEAPON_MYSTIC,        // Lightning strikes
    // New unlockable weapons
    WEAPON_SEEKER,        // Homing missiles
    WEAPON_BOOMERANG,     // Returning blade
    WEAPON_POISON,        // Toxic clouds
    WEAPON_CHAIN,         // Chain lightning
    WEAPON_COUNT
} WeaponType;

// =============================================================================
// PLAYER CLASSES
// =============================================================================

typedef enum {
    CLASS_BALANCED = 0,   // Average everything (default/classic mode)
    CLASS_WARRIOR,        // High HP, normal speed, good armor, normal XP
    CLASS_SCOUT,          // Low HP, fast speed, low armor, bonus XP gain
    CLASS_TANK,           // Very high HP, slow speed, high armor, reduced XP
    CLASS_MAGE,           // Low HP, normal speed, no armor, high XP, magic bonus
    CLASS_COUNT
} PlayerClass;

// Class stat modifiers (applied as multipliers or flat bonuses to base stats)
typedef struct {
    const char* name;           // Display name
    const char* description;    // Short description for UI

    // Base stat modifiers
    int baseHP;                 // Starting max HP
    float speedMultiplier;      // Multiplier to PLAYER_SPEED
    float armorPercent;         // Starting damage reduction (0-100)
    float xpMultiplier;         // XP gain multiplier (1.0 = normal)

    // Starting bonuses
    WeaponType preferredWeapon; // Recommended starting weapon
    float weaponDamageBonus;    // % bonus damage with preferred weapon (0 = none)

    // Visual
    Color classColor;           // Tint color for player when using this class
} ClassStats;

// Static class definitions - use CLASS_STATS[playerClass] to access
static const ClassStats CLASS_STATS[CLASS_COUNT] = {
    // CLASS_BALANCED - The default experience, no special bonuses or penalties
    {
        .name = "Balanced",
        .description = "Classic mode. Average stats, no bonuses.",
        .baseHP = 100,
        .speedMultiplier = 1.0f,
        .armorPercent = 0.0f,
        .xpMultiplier = 1.0f,
        .preferredWeapon = WEAPON_MELEE,
        .weaponDamageBonus = 0.0f,
        .classColor = {0, 220, 220, 255}  // Default cyan
    },

    // CLASS_WARRIOR - Tanky fighter with melee focus
    {
        .name = "Warrior",
        .description = "Sturdy fighter. +HP, +Armor, Melee bonus.",
        .baseHP = 130,
        .speedMultiplier = 1.0f,
        .armorPercent = 15.0f,
        .xpMultiplier = 1.0f,
        .preferredWeapon = WEAPON_MELEE,
        .weaponDamageBonus = 20.0f,  // +20% melee damage
        .classColor = {255, 150, 100, 255}  // Orange/bronze
    },

    // CLASS_SCOUT - Glass cannon with high mobility and XP gain
    {
        .name = "Scout",
        .description = "Fast & fragile. +Speed, +XP gain, -HP.",
        .baseHP = 70,
        .speedMultiplier = 1.35f,
        .armorPercent = 0.0f,
        .xpMultiplier = 1.25f,  // +25% XP gain
        .preferredWeapon = WEAPON_DISTANCE,
        .weaponDamageBonus = 15.0f,  // +15% ranged damage
        .classColor = {255, 220, 50, 255}  // Yellow
    },

    // CLASS_TANK - Extremely tanky but slow leveler
    {
        .name = "Tank",
        .description = "Immovable wall. +HP, +Armor, -Speed, -XP.",
        .baseHP = 180,
        .speedMultiplier = 0.75f,
        .armorPercent = 25.0f,
        .xpMultiplier = 0.8f,  // -20% XP gain (slower leveling)
        .preferredWeapon = WEAPON_RADIUS,
        .weaponDamageBonus = 15.0f,  // +15% orbit damage
        .classColor = {160, 80, 200, 255}  // Purple
    },

    // CLASS_MAGE - High risk/reward magic specialist
    {
        .name = "Mage",
        .description = "Magic master. +XP, Magic bonus, -HP, no armor.",
        .baseHP = 60,
        .speedMultiplier = 1.0f,
        .armorPercent = 0.0f,
        .xpMultiplier = 1.4f,  // +40% XP gain
        .preferredWeapon = WEAPON_MAGIC,
        .weaponDamageBonus = 30.0f,  // +30% magic damage
        .classColor = {200, 100, 255, 255}  // Magenta/purple
    }
};

typedef enum {
    ENEMY_WALKER = 0,
    ENEMY_FAST,
    ENEMY_TANK,
    // Wave 5+ enemies (progressively unlocked)
    ENEMY_SWARM,    // Wave 5: tiny, fast, spawns in groups
    ENEMY_ELITE,    // Wave 7: upgraded walker, more HP
    ENEMY_HORNET,   // Wave 8: ranged laser attacker
    ENEMY_BRUTE,    // Wave 10: slow heavy hitter
    // Bullet hell enemies (Wave 12+)
    ENEMY_SPINNER,  // Wave 12: spiral bullet patterns, vulnerable eye
    ENEMY_MIRROR,   // Wave 13: creates decoys, find the real one
    ENEMY_SHIELDER, // Wave 14: rotating shield, hit from behind
    ENEMY_BOSS,     // Wave 15: large, powerful, rare
    ENEMY_BOMBER,   // Wave 16: drops delayed mines
    ENEMY_PHASER,   // Wave 18: phases in/out, teleports
    ENEMY_TYPE_COUNT
} EnemyType;

// =============================================================================
// CHAMPION SYSTEM (Elite variants of regular enemies)
// =============================================================================

typedef enum {
    AFFIX_NONE = 0,
    AFFIX_SWIFT,     // +50% speed
    AFFIX_VAMPIRIC,  // Regen HP (1 HP/sec)
    AFFIX_ARMORED,   // 50% damage reduction
    AFFIX_SPLITTER,  // Spawns 2 copies on death
    AFFIX_COUNT
} EnemyAffix;

#define CHAMPION_SPAWN_WAVE 3        // Champions can spawn after wave 3
#define CHAMPION_SPAWN_CHANCE 5      // 5% chance for enemy to become champion
#define CHAMPION_XP_MULTIPLIER 2.5f  // Champions give 2.5x XP
#define CHAMPION_HP_MULTIPLIER 1.5f  // Champions have 50% more HP
#define AFFIX_SWIFT_SPEED_BONUS 0.5f // +50% speed
#define AFFIX_VAMPIRIC_REGEN 1.0f    // 1 HP per second
#define AFFIX_ARMORED_REDUCTION 0.5f // 50% damage reduction
#define AFFIX_SPLITTER_COUNT 2       // Spawn 2 copies on death
#define AFFIX_SPLITTER_HP_RATIO 0.4f // Copies have 40% of parent HP
#define AFFIX_SPLITTER_SIZE_RATIO 0.7f // Copies are 70% the size

// Starting weapons (first 5)
#define STARTING_WEAPON_COUNT 5

// Skill tree branches - each starting weapon has 3 branches
#define BRANCH_UNLOCK_TIER 2      // Must reach tier 2 to choose a branch
#define MAX_BRANCH_TIER 5

// Melee branches
typedef enum {
    MELEE_BRANCH_NONE = 0,
    MELEE_BRANCH_WIDE,      // Wide Arc - sweep wider, faster
    MELEE_BRANCH_POWER,     // Power Strike - heavy damage, knockback
    MELEE_BRANCH_SPIN       // Blade Storm - continuous spinning
} MeleeBranch;

// Distance branches
typedef enum {
    DISTANCE_BRANCH_NONE = 0,
    DISTANCE_BRANCH_RAPID,   // Rapid Fire - more bullets, faster
    DISTANCE_BRANCH_PIERCE,  // Piercing - bullets pass through
    DISTANCE_BRANCH_SPREAD   // Spread Shot - shotgun style
} DistanceBranch;

// Magic branches
typedef enum {
    MAGIC_BRANCH_NONE = 0,
    MAGIC_BRANCH_NOVA,      // Nova Blast - larger pulses
    MAGIC_BRANCH_PULSE,     // Pulse Storm - rapid small pulses
    MAGIC_BRANCH_FREEZE     // Frost Wave - slow/freeze enemies
} MagicBranch;

// Radius branches
typedef enum {
    RADIUS_BRANCH_NONE = 0,
    RADIUS_BRANCH_SHIELD,   // Guardian - defensive, blocks hits
    RADIUS_BRANCH_SWARM,    // Swarm - many small fast orbs
    RADIUS_BRANCH_HEAVY     // Heavy Orbs - few large slow orbs
} RadiusBranch;

// Mystic branches
typedef enum {
    MYSTIC_BRANCH_NONE = 0,
    MYSTIC_BRANCH_CHAIN,    // Chain - bounces between enemies
    MYSTIC_BRANCH_STORM,    // Storm - random strikes in area
    MYSTIC_BRANCH_SMITE     // Smite - single powerful strike
} MysticBranch;

typedef enum {
    // Offensive upgrades (7 types)
    UPGRADE_WEAPON_TIER = 0, // Upgrade existing weapon tier
    UPGRADE_WEAPON_UNLOCK,   // Unlock a new weapon type
    UPGRADE_DAMAGE_ALL,      // Global damage boost %
    UPGRADE_ATTACK_SPEED,    // Reduce all cooldowns %
    UPGRADE_CRIT_CHANCE,     // % chance for 2x damage
    UPGRADE_AREA_SIZE,       // +% attack radius/range
    UPGRADE_PROJECTILE_COUNT,// +1 bullets/strikes/orbs

    // Defensive upgrades (8 types)
    UPGRADE_MAX_HP,          // +flat HP
    UPGRADE_HEALTH_REGEN,    // +HP/s when stationary
    UPGRADE_MOVE_SPEED,      // +% movement speed
    UPGRADE_MAGNET_RANGE,    // +% XP pickup range
    UPGRADE_ARMOR,           // % damage reduction
    UPGRADE_LIFESTEAL,       // % of damage dealt heals
    UPGRADE_DODGE_CHANCE,    // % chance to avoid damage
    UPGRADE_THORNS,          // Reflect % damage to attackers

    // Branch upgrades (new)
    UPGRADE_BRANCH_SELECT,   // Choose a branch (at tier 2)
    UPGRADE_BRANCH_TIER,     // Upgrade branch tier

    UPGRADE_SKIP,            // Save the point (always available)
    UPGRADE_TYPE_COUNT
} UpgradeType;

typedef enum {
    XP_SMALL = 0,   // 5 XP
    XP_MEDIUM,      // 15 XP
    XP_LARGE        // 40 XP
} XPGemType;

typedef enum {
    POTION_DAMAGE = 0,   // 2x damage for 10 seconds
    POTION_SPEED,        // 1.5x speed for 15 seconds
    POTION_SHIELD,       // Invincible for 5 seconds
    POTION_MAGNET,       // 3x magnet range for 20 seconds
    POTION_COUNT
} PotionType;

typedef enum {
    DANGER_NONE = 0,
    DANGER_FIRE,      // Periodic damage
    DANGER_SLOW,      // Movement speed reduced
    DANGER_ELECTRIC,  // Rapid small damage ticks
} DangerZoneType;

typedef struct {
    Vector2 center;
    float radius;
    DangerZoneType type;
    float timer;           // Time remaining
    float xpMultiplier;    // 1.5x - 2.0x for kills inside
    bool active;
    float warningTimer;    // 3 sec warning before activation
    float damageTimer;     // For damage tick tracking
} DangerZone;

// =============================================================================
// DATA STRUCTURES
// =============================================================================

typedef struct {
    Vector2 pos;
    float angle;        // Direction in radians
    float speed;
    float baseSpeed;
    bool isMoving;
    int hp;
    int maxHp;
    int level;
    int xp;
    int xpToNextLevel;
    float invincibilityTimer;
    float hurtFlash;

    // Class system
    PlayerClass playerClass;           // Selected starting class
    float classWeaponBonus;            // Cached weapon bonus for preferred weapon

    // Stats that can be upgraded
    float magnetRange;
    float healthRegen;      // HP per second when stationary
    float damageMultiplier;
    float stationaryTime;   // Time spent not moving (for regen)
    float xpMultiplier;     // XP gain multiplier (from class)

    // New upgrade stats
    float attackSpeedMult;  // Cooldown multiplier (lower = faster)
    float critChance;       // % chance for 2x damage (0-100)
    float areaMultiplier;   // Attack range/radius multiplier
    int bonusProjectiles;   // Extra bullets/strikes/orbs
    float armor;            // % damage reduction (0-100)
    float lifesteal;        // % of damage dealt as HP (0-100)
    float dodgeChance;      // % chance to avoid damage (0-100)
    float thorns;           // % of damage reflected (0-100)

    // Upgrade points
    int upgradePoints;
} Player;

typedef struct {
    int tier;           // 0 = not unlocked, 1-5 = base tier
    int branch;         // 0 = no branch, 1-3 = chosen branch
    int branchTier;     // 0-5 = branch-specific tier
    float cooldownTimer;
    float cooldown;
    int damage;

    // Branch-specific state
    float spinTimer;        // For melee spin branch
    bool spinning;          // Currently spinning
    int pierceCount;        // For distance pierce branch
    float freezeAmount;     // For magic freeze branch
    int shieldHits;         // For radius shield branch
    int chainJumps;         // For mystic chain branch
} WeaponSkill;

typedef struct {
    Vector2 pos;
    Vector2 vel;
    float size;
    int damage;
    bool active;
    float lifetime;
} Projectile;

typedef struct {
    float angle;
    int damage;
    bool active;
} OrbitOrb;

typedef struct {
    float radius;
    float maxRadius;
    float timer;
    float duration;
    int damage;
    bool active;
} WaveAttack;

typedef struct {
    float timer;
    float duration;
    float angle;  // Direction of swing
    int damage;
    float range;
    float arc;
    bool active;
} MeleeSwing;

typedef struct {
    Vector2 pos;
    float timer;
    int damage;
    bool active;
} LightningStrike;

// Seeker missile (homing)
typedef struct {
    Vector2 pos;
    Vector2 vel;
    float angle;
    int targetIdx;
    int damage;
    float lifetime;
    bool active;
} SeekerMissile;

// Boomerang (returning blade)
typedef struct {
    Vector2 pos;
    Vector2 startPos;
    float outwardDist;
    float maxDist;
    float angle;
    float spinAngle;
    int damage;
    float size;
    bool returning;
    bool active;
} BoomerangProj;

// Poison cloud
typedef struct {
    Vector2 pos;
    float radius;
    float duration;
    float timer;
    float tickTimer;
    int damagePerTick;
    float slowPercent;
    bool active;
    float pulsePhase;
} PoisonCloud;

// Chain lightning
typedef struct {
    int hitEnemies[16];
    int hitCount;
    int currentTarget;
    int remainingJumps;
    int baseDamage;
    float currentDamage;
    float timer;
    float jumpRange;
    bool active;
} ChainLightning;

typedef struct {
    Vector2 pos;
    Vector2 vel;
    EnemyType type;
    int hp;
    int maxHp;
    float size;
    float speed;
    int damage;
    int xpValue;
    bool active;
    float hitFlash;
    // Cached direction to player (performance optimization)
    float cachedAngle;       // Direction to player in radians
    float cachedAngleTime;   // Game time when angle was calculated
    // Hornet laser state
    float laserCooldown;      // Time until can fire again
    float laserChargeTimer;   // Charging up (warning phase)
    float laserActiveTimer;   // Laser is firing
    float laserAngle;         // Direction laser is aimed
    bool laserCharging;       // Currently charging
    bool laserFiring;         // Currently firing
    // Spinner state
    float spinAngle;          // Current rotation angle
    float attackTimer;        // Cooldown/fire timer
    int bulletsFired;         // Count in current barrage
    bool isVulnerable;        // Eye is open
    float vulnerableTimer;    // Time remaining vulnerable
    // Mirror state
    bool isDecoy;             // This is a fake copy
    int realEnemyIdx;         // Index of the real mirror (for decoys)
    float revealTimer;        // Timer for brief reveal
    float splitTimer;         // Cooldown for creating decoys
    // Shielder state
    float shieldAngle;        // Direction shield is facing
    bool isCharging;          // Doing charge attack
    float chargeTimer;        // Charge cooldown/duration
    Vector2 chargeDir;        // Direction of charge
    // Bomber state
    float dropTimer;          // Mine drop cooldown
    float stunnedTimer;       // Vulnerable after dropping
    // Phaser state
    float phaseTimer;         // Phase in/out timer
    bool isPhased;            // Currently phased out (invulnerable)
    float visibility;         // 0-1 for rendering alpha
    // Slow effect tracking (for poison clouds, etc.)
    float slowTimer;          // Time remaining for slow effect
    float slowMultiplier;     // Speed multiplier (1.0 = normal, 0.5 = 50% slow)
    float baseSpeed;          // Original speed before any slows
    // Champion system
    EnemyAffix affix;         // Champion affix type (AFFIX_NONE for regular enemies)
    bool isChampion;          // Is this a champion enemy?
    float championGlow;       // Animation timer for golden pulsing glow
} Enemy;

// Enemy bullet (for bullet hell patterns)
typedef struct {
    Vector2 pos;
    Vector2 vel;
    int damage;
    float size;
    bool active;
    Color color;
} EnemyBullet;

// Mine (dropped by bomber)
typedef struct {
    Vector2 pos;
    float timer;              // Time until explosion
    float radius;
    int damage;
    bool active;
    bool exploding;           // Currently in explosion animation
    float explodeTimer;       // Explosion animation timer
} Mine;

typedef struct {
    Vector2 pos;
    Vector2 vel;
    XPGemType type;
    int value;
    bool active;
    float bobTimer;
    bool magnetized;
    float glowTimer;      // Animation timer for glow pulse
    float sparkleTimer;   // Timer for sparkle effect
} XPGem;

// Text popup for XP collection feedback
typedef struct {
    Vector2 pos;
    Vector2 vel;
    char text[16];
    Color color;
    float life;
    float maxLife;
    float scale;
    bool active;
} TextPopup;

// UI particle that flies toward HUD
typedef struct {
    Vector2 pos;
    Vector2 target;
    Color color;
    float life;
    float speed;
    bool active;
} UIParticle;

typedef struct {
    Vector2 pos;
    Vector2 vel;
    PotionType type;
    bool active;
    float bobTimer;
} Potion;

typedef struct {
    PotionType type;
    bool active;
} InventoryPotion;

typedef struct {
    PotionType type;
    float duration;
    float timer;
    int hitCount;    // For hit-based buffs
    int maxHits;
    bool active;
} ActiveBuff;

typedef struct {
    Vector2 pos;
    Vector2 vel;
    Color color;
    float size;
    float life;
    float maxLife;
    bool active;
} Particle;

typedef struct {
    UpgradeType type;
    WeaponType weapon;  // For weapon upgrades (tier or unlock)
    int branch;         // For branch selection/upgrade (1-3)
    char name[32];
    char desc[64];
    int cost;           // Upgrade points required
    int value;          // Generic value (HP amount, % boost, etc)
    bool available;     // Can be selected (has enough points)
    bool isOffensive;   // For visual distinction
} UpgradeChoice;

typedef struct {
    float spawnTimer;
    float spawnInterval;
    int wave;
    float waveTimer;
    float difficultyMultiplier;
} SpawnSystem;

typedef struct {
    Vector2 pos;
    Vector2 target;
} GameCamera;

typedef struct {
    GameState state;
    Player player;
    GameCamera camera;

    // Class and weapon selection
    PlayerClass selectedClass;      // Currently selected class in menu
    int classSelectIndex;           // UI index for class selection carousel
    WeaponType startingWeapon;
    int weaponSelectIndex;

    // Weapon skill trees (tier 0 = locked, 1-5 = level)
    WeaponSkill weapons[WEAPON_COUNT];

    // Active weapon effects
    Projectile projectiles[MAX_PROJECTILES];
    OrbitOrb orbitOrbs[MAX_ORBIT_ORBS];
    WaveAttack wave;
    MeleeSwing melee;
    LightningStrike lightning[MAX_LIGHTNING_STRIKES];
    float lightningTimer;

    // New weapon effects
    SeekerMissile seekers[MAX_SEEKERS];
    BoomerangProj boomerangs[MAX_BOOMERANGS];
    PoisonCloud poisonClouds[MAX_POISON_CLOUDS];
    ChainLightning chains[MAX_CHAINS];

    // Enemies and pickups
    Enemy enemies[MAX_ENEMIES];
    XPGem xpGems[MAX_XP_GEMS];
    Potion potions[MAX_POTIONS];

    // Enemy projectiles (bullet hell)
    EnemyBullet enemyBullets[MAX_ENEMY_BULLETS];
    Mine mines[MAX_MINES];

    // Player inventory
    InventoryPotion inventory[MAX_INVENTORY_POTIONS];
    ActiveBuff buffs[POTION_COUNT];

    // Effects
    Particle particles[MAX_PARTICLES];
    float screenShake;
    float screenShakeX;
    float screenShakeY;

    // XP collection feedback
    TextPopup popups[MAX_POPUPS];
    UIParticle uiParticles[MAX_UI_PARTICLES];
    int xpCombo;
    float comboTimer;
    float screenFlash;
    Color screenFlashColor;
    float xpBarPulse;

    // Spawning
    SpawnSystem spawner;

    // Level up - carousel system with multi-purchase
    UpgradeChoice upgrades[NUM_UPGRADE_CHOICES + 1];  // +1 for skip option
    int selectedUpgrade;
    float carouselOffset;    // Smooth scrolling offset for animation
    float targetOffset;      // Target offset for smooth lerp
    int selectedPotion;      // For using potions during level up
    int sessionPointsRemaining;                       // Points left this session
    bool upgradesPurchasedThisSession[NUM_UPGRADE_CHOICES + 1];  // Track purchases
    int levelUpMode;         // 0=carousel, 1=confirm button selected
    float purchaseFlashTimer; // Flash timer for purchase feedback

    // Stats
    float gameTime;
    int killCount;
    int highestWave;

    // Animation
    float bgTime;
    int menuIndex;

    // Graze system
    int grazeCount;           // Total grazes this run
    float grazeFlash;         // Visual flash timer for graze
    int grazeCombo;           // Consecutive grazes
    float grazeComboTimer;    // Reset combo if no graze

    // Kill combo system (tiered bonuses)
    int killCombo;            // Current kill combo count
    float killComboTimer;     // 2.5 sec timeout
    ComboTier comboTier;      // Current tier (COMBO_NONE to COMBO_GODLIKE)
    ComboTier prevComboTier;  // Previous tier (for detecting tier changes)
    float comboTierFlash;     // Flash when tier changes
    int highestCombo;         // Best combo this run

    // Danger Zones
    DangerZone dangerZones[MAX_DANGER_ZONES];
    float dangerZoneSpawnTimer;

    // Kill milestone rewards
    KillMilestone milestones[MAX_MILESTONES];
    int nextMilestoneIdx;
    float milestoneFlash;
    float milestoneCelebrationTimer;
} Game;

// =============================================================================
// API FUNCTIONS
// =============================================================================

// Core game functions
void GameInit(int width, int height);
void GameUpdate(const LlzInputState *input, float dt);
void GameDraw(void);
void GameShutdown(void);
void GameReset(void);

// State checks
bool GameWantsClose(void);

#ifdef __cplusplus
}
#endif

#endif // LLZSURVIVORS_GAME_H
