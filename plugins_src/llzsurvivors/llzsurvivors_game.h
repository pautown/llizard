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

// HP scaling: HP = BASE_HP + (gameTime * HP_SCALE_RATE)
#define HP_SCALE_RATE 0.15f

// XP Gems
#define MAX_XP_GEMS 128
#define XP_GEM_SIZE 8.0f
#define XP_GEM_MAGNET_SPEED 350.0f

// XP Popups and UI effects
#define MAX_POPUPS 32
#define MAX_UI_PARTICLES 32

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

// =============================================================================
// ENUMERATIONS
// =============================================================================

typedef enum {
    GAME_STATE_MENU = 0,
    GAME_STATE_WEAPON_SELECT,
    GAME_STATE_PLAYING,
    GAME_STATE_LEVEL_UP,
    GAME_STATE_PAUSED,
    GAME_STATE_GAME_OVER
} GameState;

typedef enum {
    ENEMY_WALKER = 0,
    ENEMY_FAST,
    ENEMY_TANK
} EnemyType;

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

    // Stats that can be upgraded
    float magnetRange;
    float healthRegen;      // HP per second when stationary
    float damageMultiplier;
    float stationaryTime;   // Time spent not moving (for regen)

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
} Enemy;

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

    // Starting weapon choice
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

    // Level up - carousel system
    UpgradeChoice upgrades[NUM_UPGRADE_CHOICES + 1];  // +1 for skip option
    int selectedUpgrade;
    float carouselOffset;    // Smooth scrolling offset for animation
    float targetOffset;      // Target offset for smooth lerp
    int selectedPotion;      // For using potions during level up

    // Stats
    float gameTime;
    int killCount;
    int highestWave;

    // Animation
    float bgTime;
    int menuIndex;
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
