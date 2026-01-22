// Cauldron Cascade - The Grand Mysterium
// Gold becoming aware of itself becoming gold

#include "llizard_plugin.h"
#include "llz_sdk.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

#define GRID_WIDTH 6
#define GRID_HEIGHT 8
#define CELL_SIZE 48
#define CELL_GAP 2
#define MERGE_THRESHOLD 3

// ============================================================================
// BREATH - Smooth, living motion
// ============================================================================

static float Ease(float t) {
    return t < 0.5f ? 2 * t * t : 1 - powf(-2 * t + 2, 2) / 2;
}

static float Breathe(float t, float rate) {
    return 0.5f + 0.5f * sinf(t * rate);
}

static float Approach(float current, float target, float rate, float dt) {
    float diff = target - current;
    return current + diff * fminf(1.0f, rate * dt);
}

// ============================================================================
// ELEMENTS - The substances of transformation
// ============================================================================

typedef enum {
    ELEM_EMPTY = 0,
    // Prima Materia
    ELEM_FIRE = 1, ELEM_WATER, ELEM_EARTH, ELEM_AIR,
    // First Works
    ELEM_STEAM, ELEM_LAVA, ELEM_SMOKE, ELEM_MUD, ELEM_RAIN, ELEM_DUST,
    ELEM_SALT, ELEM_SULFUR, ELEM_MERCURY, ELEM_VOID, ELEM_SPARK, ELEM_ICE,
    ELEM_LIGHTNING, ELEM_MAGMA, ELEM_MIST, ELEM_CLAY,
    // Materia
    ELEM_STONE, ELEM_METAL, ELEM_CRYSTAL, ELEM_PLANT, ELEM_CLOUD, ELEM_GLASS,
    ELEM_OBSIDIAN, ELEM_SAND, ELEM_ASH, ELEM_COAL, ELEM_WOOD, ELEM_SEED,
    ELEM_FLOWER, ELEM_FRUIT, ELEM_FUNGUS, ELEM_MOSS, ELEM_VINE, ELEM_CORAL,
    ELEM_PEARL, ELEM_AMBER,
    // Vita
    ELEM_LIFE, ELEM_CELL, ELEM_BLOOD, ELEM_BONE, ELEM_FLESH, ELEM_BREATH,
    ELEM_HEART, ELEM_EYE, ELEM_BRAIN, ELEM_NERVE, ELEM_MUSCLE, ELEM_SKIN,
    ELEM_HAIR, ELEM_TEAR, ELEM_SWEAT, ELEM_VENOM, ELEM_NECTAR, ELEM_SAP,
    ELEM_POLLEN, ELEM_SPORE,
    // Anima
    ELEM_MIND, ELEM_THOUGHT, ELEM_MEMORY, ELEM_DREAM, ELEM_EMOTION,
    ELEM_FEAR, ELEM_JOY, ELEM_SORROW, ELEM_ANGER, ELEM_LOVE, ELEM_HOPE,
    ELEM_DESPAIR, ELEM_CURIOSITY, ELEM_WISDOM, ELEM_INTUITION, ELEM_INSTINCT,
    ELEM_WILL, ELEM_DESIRE, ELEM_CONSCIENCE, ELEM_EGO,
    // Spiritus
    ELEM_AETHER, ELEM_SPIRIT, ELEM_SOUL, ELEM_MANA, ELEM_CHI, ELEM_PRANA,
    ELEM_KARMA, ELEM_DHARMA, ELEM_TIME, ELEM_SPACE, ELEM_LIGHT, ELEM_SHADOW,
    ELEM_CHAOS, ELEM_ORDER, ELEM_YIN, ELEM_YANG, ELEM_BALANCE, ELEM_VIBRATION,
    ELEM_RESONANCE, ELEM_HARMONY,
    // Chakras
    ELEM_ROOT, ELEM_SACRAL, ELEM_SOLAR, ELEM_HEART_CHAKRA, ELEM_THROAT,
    ELEM_THIRD_EYE, ELEM_CROWN,
    // Cosmos
    ELEM_STAR, ELEM_MOON, ELEM_SUN, ELEM_COSMOS, ELEM_NEBULA, ELEM_GALAXY,
    ELEM_QUASAR, ELEM_PULSAR, ELEM_BLACK_HOLE, ELEM_WHITE_HOLE, ELEM_SINGULARITY,
    ELEM_VOID_STAR, ELEM_AKASHA,
    // Arcana
    ELEM_FOOL, ELEM_MAGICIAN, ELEM_PRIESTESS, ELEM_EMPRESS, ELEM_EMPEROR,
    ELEM_HIEROPHANT, ELEM_LOVERS, ELEM_CHARIOT, ELEM_STRENGTH, ELEM_HERMIT,
    ELEM_WHEEL, ELEM_JUSTICE, ELEM_HANGED, ELEM_DEATH, ELEM_TEMPERANCE,
    ELEM_DEVIL, ELEM_TOWER, ELEM_STAR_TAROT, ELEM_MOON_TAROT, ELEM_SUN_TAROT,
    ELEM_JUDGEMENT, ELEM_WORLD,
    // Opus
    ELEM_PRIMA_MATERIA, ELEM_NIGREDO, ELEM_ALBEDO, ELEM_CITRINITAS, ELEM_RUBEDO,
    ELEM_LAPIS, ELEM_TINCTURE, ELEM_QUINTESSENCE,
    // Mysterium
    ELEM_PHILOSOPHER, ELEM_ELIXIR, ELEM_AZOTH, ELEM_HOMUNCULUS, ELEM_GOLEM,
    ELEM_EGREGORE, ELEM_PLEROMA, ELEM_MONAD, ELEM_TAO, ELEM_OUROBOROS,
    // Essentia
    ELEM_HEAT, ELEM_SOUND, ELEM_TRUTH, ELEM_KUNDALINI, ELEM_SPIN,
    ELEM_TRADITION, ELEM_COURAGE, ELEM_SOLITUDE, ELEM_SACRIFICE, ELEM_GOLD,
    ELEM_COUNT
} ElementType;

typedef struct {
    const char *name;
    const char *glyph;
    const char *desc;  // Element description
    Color color;
    int tier;
    int weight;
    bool alive;
} Element;

// Golden palette - warm amber tones with depth
static Element g_elem[] = {
    {"", "", "", {0,0,0,0}, -1, 0, false},
    // Prima Materia - pure, bright
    {"Fire", "Fi", "The primal flame. Catalyst of change.", {255,140,60,255}, 0, 1, false},
    {"Water", "Wa", "Flow and form. The universal solvent.", {100,160,200,255}, 0, 1, false},
    {"Earth", "Ea", "Foundation and stability. The vessel.", {160,120,80,255}, 0, 1, false},
    {"Air", "Ai", "Breath and movement. The invisible force.", {200,210,220,255}, 0, 1, false},
    // First Works
    {"Steam", "St", "Fire meets water. The first vapor.", {210,215,220,255}, 1, 3, false},
    {"Lava", "Lv", "Earth's molten blood.", {255,100,40,255}, 1, 3, false},
    {"Smoke", "Sk", "Fire's ghost. What remains when form burns.", {120,115,110,255}, 1, 3, false},
    {"Mud", "Md", "The prima materia of life.", {120,90,60,255}, 1, 3, false},
    {"Rain", "Rn", "Water that remembers the sky.", {140,170,200,255}, 1, 3, false},
    {"Dust", "Du", "What all things become.", {180,165,140,255}, 1, 3, false},
    {"Salt", "Sa", "Body of the work. Fixed principle.", {250,248,245,255}, 1, 4, false},
    {"Sulfur", "Su", "Soul of the work. Active principle.", {220,200,80,255}, 1, 4, true},
    {"Mercury", "Hg", "Spirit of the work. Binding principle.", {200,200,210,255}, 1, 4, true},
    {"Void", "Vo", "The space between. Potential.", {30,25,35,255}, 1, 5, false},
    {"Spark", "Sp", "A fragment of the first fire.", {255,230,150,255}, 1, 3, true},
    {"Ice", "Ic", "Water's memory of stillness.", {200,220,235,255}, 1, 3, false},
    {"Lightning", "Lt", "Fire that falls from heaven.", {255,250,200,255}, 1, 5, true},
    {"Magma", "Mg", "Earth remembering its birth.", {255,120,50,255}, 1, 4, false},
    {"Mist", "Mi", "Water dreaming of air.", {210,215,220,255}, 1, 3, true},
    {"Clay", "Cy", "Earth waiting to be shaped.", {175,145,115,255}, 1, 3, false},
    // Materia
    {"Stone", "Sn", "Time made solid.", {140,135,130,255}, 2, 5, false},
    {"Metal", "Mt", "Earth's hidden strength.", {180,175,170,255}, 2, 7, false},
    {"Crystal", "Cr", "Light trapped in geometry.", {200,190,220,255}, 2, 7, false},
    {"Plant", "Pl", "The green work. Life reaching upward.", {90,140,70,255}, 2, 5, true},
    {"Cloud", "Cl", "Water that has learned to fly.", {235,235,240,255}, 2, 5, true},
    {"Glass", "Gl", "Sand's transformation. Clarity.", {215,225,235,255}, 2, 6, false},
    {"Obsidian", "Ob", "Volcanic glass. The dark mirror.", {40,35,45,255}, 2, 8, false},
    {"Sand", "Sd", "Time's patient work on stone.", {225,205,170,255}, 2, 4, false},
    {"Ash", "As", "What fire leaves behind.", {150,145,140,255}, 2, 4, false},
    {"Coal", "Co", "Ancient sunlight, compressed.", {50,45,40,255}, 2, 6, false},
    {"Wood", "Wd", "Life's structure.", {150,110,70,255}, 2, 4, false},
    {"Seed", "Se", "Potential waiting.", {150,130,80,255}, 2, 5, true},
    {"Flower", "Fl", "Beauty's brief statement.", {230,170,190,255}, 2, 6, true},
    {"Fruit", "Fr", "Promise fulfilled.", {220,130,100,255}, 2, 7, false},
    {"Fungus", "Fu", "The decomposer. The recycler.", {160,140,120,255}, 2, 5, true},
    {"Moss", "Ms", "Patient green. First colonizer.", {110,140,90,255}, 2, 4, true},
    {"Vine", "Vn", "Life that climbs.", {100,150,80,255}, 2, 5, false},
    {"Coral", "Cr", "Stone that lives.", {230,160,140,255}, 2, 6, false},
    {"Pearl", "Pr", "Irritation made beautiful.", {250,248,245,255}, 2, 8, false},
    {"Amber", "Am", "Time's golden tear.", {235,180,80,255}, 2, 7, false},
    // Vita
    {"Life", "Li", "The animating spark.", {160,200,140,255}, 3, 10, true},
    {"Cell", "Ce", "The smallest vessel of life.", {140,190,160,255}, 3, 8, true},
    {"Blood", "Bl", "The river within.", {180,60,60,255}, 3, 12, false},
    {"Bone", "Bo", "The frame that remembers.", {250,245,235,255}, 3, 9, false},
    {"Flesh", "Fs", "Matter that feels.", {240,200,180,255}, 3, 10, false},
    {"Breath", "Br", "Life's rhythm.", {220,230,240,255}, 3, 8, true},
    {"Heart", "Ht", "The tireless drum.", {200,80,90,255}, 3, 15, true},
    {"Eye", "Ey", "Light's interpreter.", {130,150,180,255}, 3, 12, false},
    {"Brain", "Bn", "The labyrinth of thought.", {240,210,200,255}, 3, 18, false},
    {"Nerve", "Nv", "Lightning of the body.", {250,245,200,255}, 3, 10, true},
    {"Muscle", "Mu", "Will made movement.", {190,120,110,255}, 3, 8, false},
    {"Skin", "Sk", "The boundary of self.", {245,220,200,255}, 3, 7, false},
    {"Hair", "Hr", "The body's memory.", {80,60,50,255}, 3, 5, false},
    {"Tear", "Tr", "Emotion made water.", {200,210,230,255}, 3, 8, false},
    {"Sweat", "Sw", "The salt of effort.", {220,215,200,255}, 3, 4, false},
    {"Venom", "Vm", "Defense made liquid.", {130,180,90,255}, 3, 12, false},
    {"Nectar", "Nc", "Sweetness that summons.", {245,200,120,255}, 3, 10, false},
    {"Sap", "Sp", "Blood of trees.", {200,160,80,255}, 3, 6, false},
    {"Pollen", "Po", "Life's golden dust.", {250,225,130,255}, 3, 7, false},
    {"Spore", "Sr", "Patience in a shell.", {160,175,145,255}, 3, 8, true},
    // Anima
    {"Mind", "Mn", "The inner alchemist.", {180,170,200,255}, 4, 15, true},
    {"Thought", "Th", "Mind's offspring.", {200,195,215,255}, 4, 12, true},
    {"Memory", "Me", "Time captured.", {170,160,190,255}, 4, 14, false},
    {"Dream", "Dr", "The mind's laboratory.", {190,180,210,255}, 4, 15, true},
    {"Emotion", "Em", "The color of experience.", {220,180,190,255}, 4, 13, true},
    {"Fear", "Fe", "The shadow of survival.", {100,95,110,255}, 4, 10, true},
    {"Joy", "Jo", "Light made feeling.", {250,220,140,255}, 4, 14, false},
    {"Sorrow", "So", "Depth known.", {130,140,160,255}, 4, 11, false},
    {"Anger", "An", "Fire of the heart.", {220,110,100,255}, 4, 12, true},
    {"Love", "Lv", "The great dissolver of boundaries.", {230,150,170,255}, 4, 20, false},
    {"Hope", "Ho", "Light not yet arrived.", {250,245,210,255}, 4, 16, false},
    {"Despair", "Ds", "The dark night.", {80,75,90,255}, 4, 10, false},
    {"Curiosity", "Cu", "The beginning of wisdom.", {200,220,190,255}, 4, 14, true},
    {"Wisdom", "Wi", "Knowledge transformed.", {240,215,180,255}, 4, 25, false},
    {"Intuition", "In", "Knowing without knowing.", {200,190,215,255}, 4, 18, true},
    {"Instinct", "Is", "Ancient wisdom.", {180,190,160,255}, 4, 12, false},
    {"Will", "Wl", "The first mover.", {240,200,130,255}, 4, 20, false},
    {"Desire", "De", "The fuel of becoming.", {230,140,160,255}, 4, 15, true},
    {"Conscience", "Cn", "The inner voice.", {250,250,230,255}, 4, 18, false},
    {"Ego", "Eg", "The one who asks 'who am I?'", {200,170,140,255}, 4, 12, true},
    // Spiritus
    {"Aether", "Ae", "The fifth element.", {190,200,220,255}, 5, 25, false},
    {"Spirit", "Sp", "The breath of the divine.", {250,250,250,255}, 5, 30, true},
    {"Soul", "So", "The eternal witness.", {245,230,245,255}, 5, 35, false},
    {"Mana", "Ma", "The fuel of magic.", {140,170,220,255}, 5, 28, false},
    {"Chi", "Ch", "Life force flowing.", {250,250,210,255}, 5, 26, true},
    {"Prana", "Pr", "Breath of the cosmos.", {250,210,170,255}, 5, 28, true},
    {"Karma", "Ka", "Action's echo.", {200,190,210,255}, 5, 32, false},
    {"Dharma", "Dh", "The way that must be walked.", {250,230,200,255}, 5, 35, false},
    {"Time", "Ti", "The river that flows one way.", {190,185,200,255}, 5, 40, true},
    {"Space", "Sc", "The vessel of all.", {120,110,150,255}, 5, 40, false},
    {"Light", "Lg", "The first word.", {255,252,245,255}, 5, 30, false},
    {"Shadow", "Sh", "Light's faithful companion.", {60,55,70,255}, 5, 30, false},
    {"Chaos", "Ca", "Infinite possibility.", {230,100,130,255}, 5, 35, true},
    {"Order", "Or", "Pattern made manifest.", {140,180,210,255}, 5, 35, false},
    {"Yin", "Yi", "The receptive darkness.", {50,45,60,255}, 5, 25, false},
    {"Yang", "Ya", "The active light.", {255,252,245,255}, 5, 25, false},
    {"Balance", "Ba", "The still point.", {190,185,180,255}, 5, 40, false},
    {"Vibration", "Vb", "All is frequency.", {200,175,210,255}, 5, 22, true},
    {"Resonance", "Rs", "When frequencies align.", {175,195,220,255}, 5, 28, true},
    {"Harmony", "Ha", "Many becoming one.", {200,220,195,255}, 5, 35, false},
    // Chakras - rainbow, muted
    {"Root", "Rt", "Foundation. Survival. The red wheel.", {200,90,90,255}, 6, 40, false},
    {"Sacral", "Sc", "Creation. Emotion. The orange wheel.", {220,150,100,255}, 6, 42, false},
    {"Solar", "Sl", "Power. Will. The yellow wheel.", {230,210,120,255}, 6, 44, false},
    {"Heart", "Hc", "Love. Connection. The green wheel.", {120,180,130,255}, 6, 46, false},
    {"Throat", "Tt", "Expression. Truth. The blue wheel.", {120,170,200,255}, 6, 48, false},
    {"Third Eye", "Te", "Insight. Vision. The indigo wheel.", {130,120,170,255}, 6, 50, true},
    {"Crown", "Cw", "Unity. Transcendence. The violet wheel.", {200,170,200,255}, 6, 55, true},
    // Cosmos - deep, distant
    {"Star", "Sr", "Distant fire.", {255,250,220,255}, 7, 50, true},
    {"Moon", "Mn", "Reflected light. The feminine.", {230,230,240,255}, 7, 55, true},
    {"Sun", "Sn", "The heart of the sky.", {255,230,150,255}, 7, 60, false},
    {"Cosmos", "Cs", "The infinite dark.", {40,35,60,255}, 7, 70, false},
    {"Nebula", "Nb", "Star nursery.", {170,140,190,255}, 7, 65, true},
    {"Galaxy", "Gx", "Island universe.", {120,110,160,255}, 7, 75, false},
    {"Quasar", "Qr", "Light from the beginning.", {230,210,240,255}, 7, 80, true},
    {"Pulsar", "Pu", "The cosmic heartbeat.", {210,230,240,255}, 7, 78, true},
    {"Black Hole", "BH", "Where light goes to forget.", {20,15,25,255}, 7, 90, false},
    {"White Hole", "WH", "Where light is born anew.", {255,255,255,255}, 7, 90, false},
    {"Singularity", "Si", "The point where physics dreams.", {255,255,255,255}, 7, 100, true},
    {"Void Star", "VS", "The space between stars.", {100,90,130,255}, 7, 85, false},
    {"Akasha", "Ak", "The cosmic memory.", {200,180,220,255}, 7, 95, false},
    // Arcana - aged parchment tones
    {"Fool", "0", "The beginning. Pure potential.", {250,245,220,255}, 8, 50, true},
    {"Magician", "I", "Will made manifest.", {220,140,130,255}, 8, 55, false},
    {"Priestess", "II", "Hidden knowledge.", {170,175,210,255}, 8, 60, true},
    {"Empress", "III", "Creation. Abundance.", {160,200,150,255}, 8, 65, false},
    {"Emperor", "IV", "Structure. Authority.", {220,170,120,255}, 8, 65, false},
    {"Hierophant", "V", "Tradition. Teaching.", {200,185,165,255}, 8, 60, false},
    {"Lovers", "VI", "Choice. Union.", {230,190,195,255}, 8, 70, false},
    {"Chariot", "VII", "Victory through will.", {190,195,215,255}, 8, 65, false},
    {"Strength", "VIII", "Gentle power.", {235,200,140,255}, 8, 70, false},
    {"Hermit", "IX", "Inner light.", {165,160,170,255}, 8, 75, false},
    {"Wheel", "X", "Fate's turning.", {200,175,200,255}, 8, 80, true},
    {"Justice", "XI", "Balance restored.", {245,240,210,255}, 8, 75, false},
    {"Hanged", "XII", "Surrender. New perspective.", {175,195,215,255}, 8, 70, true},
    {"Death", "XIII", "Transformation. Ending.", {70,65,75,255}, 8, 85, false},
    {"Temperance", "XIV", "Alchemy. Integration.", {200,210,225,255}, 8, 80, false},
    {"Devil", "XV", "Bondage. Shadow work.", {120,80,85,255}, 8, 75, true},
    {"Tower", "XVI", "Sudden change. Revelation.", {220,140,100,255}, 8, 90, true},
    {"Star", "XVII", "Hope. Inspiration.", {250,250,230,255}, 8, 85, false},
    {"Moon", "XVIII", "Illusion. The unconscious.", {210,210,230,255}, 8, 80, true},
    {"Sun", "XIX", "Joy. Success.", {255,235,150,255}, 8, 90, false},
    {"Judgement", "XX", "Awakening. Calling.", {250,230,200,255}, 8, 95, false},
    {"World", "XXI", "Completion. Integration.", {190,220,200,255}, 8, 100, false},
    // Opus - the stages of the work
    {"Prima Materia", "PM", "The raw material. Chaos before form.", {100,90,110,255}, 9, 100, true},
    {"Nigredo", "Ng", "The blackening. Death of the old.", {35,30,35,255}, 9, 110, false},
    {"Albedo", "Ab", "The whitening. Purification.", {252,252,255,255}, 9, 120, false},
    {"Citrinitas", "Ct", "The yellowing. Dawn approaches.", {255,225,130,255}, 9, 130, false},
    {"Rubedo", "Rb", "The reddening. Completion nears.", {200,90,90,255}, 9, 150, false},
    {"Lapis", "Lp", "The stone. Nearly gold.", {130,115,180,255}, 9, 140, false},
    {"Tincture", "Tn", "The stain that transforms.", {230,140,160,255}, 9, 135, false},
    {"Quintessence", "Qn", "The fifth essence. Pure spirit.", {255,255,255,255}, 9, 160, false},
    // Mysterium - the final attainments
    {"Philosopher's Stone", "Au", "Gold that makes gold.", {255,215,100,255}, 10, 250, false},
    {"Elixir", "Ex", "Life everlasting.", {150,230,180,255}, 10, 250, false},
    {"Azoth", "Az", "The universal medicine.", {210,220,240,255}, 10, 250, false},
    {"Homunculus", "Hm", "Life from art.", {245,215,200,255}, 10, 200, true},
    {"Golem", "Gm", "Earth awakened.", {160,145,130,255}, 10, 180, false},
    {"Egregore", "Eg", "Thought made being.", {210,200,230,255}, 10, 220, true},
    {"Pleroma", "Pl", "Fullness of the divine.", {255,255,255,255}, 10, 280, false},
    {"Monad", "Mo", "The One.", {255,255,255,255}, 10, 300, false},
    {"Tao", "To", "The way that cannot be named.", {210,205,200,255}, 10, 350, false},
    {"Ouroboros", "Ou", "The serpent eating its tail.", {200,190,180,255}, 10, 500, true},
    // Essentia
    {"Heat", "Ht", "Fire's invisible gift.", {255,180,130,255}, 1, 4, true},
    {"Sound", "Sd", "Air that remembers.", {195,205,215,255}, 2, 8, true},
    {"Truth", "Tr", "What remains when lies burn.", {255,255,245,255}, 4, 22, false},
    {"Kundalini", "Ku", "The coiled serpent of energy.", {230,120,160,255}, 6, 60, true},
    {"Spin", "Sn", "Motion without end.", {205,215,230,255}, 5, 20, true},
    {"Tradition", "Td", "Wisdom passed down.", {190,175,160,255}, 4, 18, false},
    {"Courage", "Cg", "Fear transformed.", {240,190,130,255}, 4, 20, false},
    {"Solitude", "Sl", "The alchemist's companion.", {145,145,160,255}, 4, 16, false},
    {"Sacrifice", "Sc", "What is given becomes gold.", {190,100,110,255}, 4, 24, false},
    {"Gold", "Au", "The perfected metal. The goal.", {255,215,100,255}, 2, 25, false},
};

// ============================================================================
// RECIPES - The combinations
// ============================================================================

typedef struct { ElementType a, b, r; } Recipe;

static Recipe g_recipes[] = {
    // Prima Materia
    {ELEM_FIRE, ELEM_WATER, ELEM_STEAM}, {ELEM_FIRE, ELEM_EARTH, ELEM_LAVA},
    {ELEM_FIRE, ELEM_AIR, ELEM_SMOKE}, {ELEM_WATER, ELEM_EARTH, ELEM_MUD},
    {ELEM_WATER, ELEM_AIR, ELEM_RAIN}, {ELEM_EARTH, ELEM_AIR, ELEM_DUST},
    {ELEM_FIRE, ELEM_FIRE, ELEM_SPARK}, {ELEM_WATER, ELEM_WATER, ELEM_ICE},
    {ELEM_EARTH, ELEM_EARTH, ELEM_STONE}, {ELEM_AIR, ELEM_AIR, ELEM_VOID},
    // Tria Prima
    {ELEM_STONE, ELEM_WATER, ELEM_SALT}, {ELEM_FIRE, ELEM_STONE, ELEM_SULFUR},
    {ELEM_WATER, ELEM_METAL, ELEM_MERCURY}, {ELEM_SALT, ELEM_SULFUR, ELEM_PHILOSOPHER},
    {ELEM_MERCURY, ELEM_SULFUR, ELEM_PHILOSOPHER}, {ELEM_SALT, ELEM_MERCURY, ELEM_ELIXIR},
    // Materia
    {ELEM_LAVA, ELEM_WATER, ELEM_STONE}, {ELEM_LAVA, ELEM_AIR, ELEM_OBSIDIAN},
    {ELEM_STONE, ELEM_FIRE, ELEM_METAL}, {ELEM_STONE, ELEM_AIR, ELEM_SAND},
    {ELEM_SAND, ELEM_FIRE, ELEM_GLASS}, {ELEM_STONE, ELEM_VOID, ELEM_CRYSTAL},
    {ELEM_STEAM, ELEM_AIR, ELEM_CLOUD}, {ELEM_CLOUD, ELEM_SPARK, ELEM_LIGHTNING},
    {ELEM_MUD, ELEM_EARTH, ELEM_CLAY},
    // Natura
    {ELEM_MUD, ELEM_RAIN, ELEM_PLANT}, {ELEM_PLANT, ELEM_EARTH, ELEM_WOOD},
    {ELEM_PLANT, ELEM_WATER, ELEM_SEED}, {ELEM_SEED, ELEM_RAIN, ELEM_FLOWER},
    {ELEM_FLOWER, ELEM_SUN, ELEM_FRUIT}, {ELEM_PLANT, ELEM_VOID, ELEM_FUNGUS},
    {ELEM_PLANT, ELEM_TIME, ELEM_AMBER}, {ELEM_PLANT, ELEM_FIRE, ELEM_ASH},
    {ELEM_WOOD, ELEM_FIRE, ELEM_COAL}, {ELEM_STONE, ELEM_LIFE, ELEM_CORAL},
    {ELEM_CORAL, ELEM_SAND, ELEM_PEARL},
    // Vita
    {ELEM_PLANT, ELEM_RAIN, ELEM_LIFE}, {ELEM_LIFE, ELEM_WATER, ELEM_CELL},
    {ELEM_CELL, ELEM_CELL, ELEM_LIFE}, {ELEM_LIFE, ELEM_FIRE, ELEM_BLOOD},
    {ELEM_LIFE, ELEM_STONE, ELEM_BONE}, {ELEM_LIFE, ELEM_CLAY, ELEM_FLESH},
    {ELEM_LIFE, ELEM_AIR, ELEM_BREATH}, {ELEM_BLOOD, ELEM_LIFE, ELEM_HEART},
    {ELEM_LIFE, ELEM_LIGHT, ELEM_EYE}, {ELEM_LIFE, ELEM_LIGHTNING, ELEM_BRAIN},
    {ELEM_BRAIN, ELEM_SPARK, ELEM_NERVE}, {ELEM_LIFE, ELEM_HEAT, ELEM_SWEAT},
    // Anima
    {ELEM_BRAIN, ELEM_SPARK, ELEM_MIND}, {ELEM_MIND, ELEM_AIR, ELEM_THOUGHT},
    {ELEM_MIND, ELEM_TIME, ELEM_MEMORY}, {ELEM_MIND, ELEM_VOID, ELEM_DREAM},
    {ELEM_MIND, ELEM_WATER, ELEM_EMOTION}, {ELEM_EMOTION, ELEM_VOID, ELEM_FEAR},
    {ELEM_EMOTION, ELEM_LIGHT, ELEM_JOY}, {ELEM_EMOTION, ELEM_FIRE, ELEM_ANGER},
    {ELEM_EMOTION, ELEM_LIFE, ELEM_LOVE}, {ELEM_JOY, ELEM_LIGHT, ELEM_HOPE},
    {ELEM_MEMORY, ELEM_TIME, ELEM_WISDOM}, {ELEM_MIND, ELEM_FIRE, ELEM_WILL},
    // Spiritus
    {ELEM_VOID, ELEM_SPARK, ELEM_AETHER}, {ELEM_LIFE, ELEM_AETHER, ELEM_SPIRIT},
    {ELEM_SPIRIT, ELEM_MIND, ELEM_SOUL}, {ELEM_AETHER, ELEM_CRYSTAL, ELEM_MANA},
    {ELEM_BREATH, ELEM_SPIRIT, ELEM_CHI}, {ELEM_LIFE, ELEM_BREATH, ELEM_PRANA},
    {ELEM_SOUL, ELEM_TIME, ELEM_KARMA}, {ELEM_SOUL, ELEM_WISDOM, ELEM_DHARMA},
    {ELEM_VOID, ELEM_LIGHT, ELEM_TIME}, {ELEM_VOID, ELEM_VOID, ELEM_SPACE},
    {ELEM_FIRE, ELEM_AETHER, ELEM_LIGHT}, {ELEM_VOID, ELEM_LIGHT, ELEM_SHADOW},
    {ELEM_VOID, ELEM_LIGHTNING, ELEM_CHAOS}, {ELEM_CRYSTAL, ELEM_LIGHT, ELEM_ORDER},
    {ELEM_SHADOW, ELEM_WATER, ELEM_YIN}, {ELEM_LIGHT, ELEM_FIRE, ELEM_YANG},
    {ELEM_YIN, ELEM_YANG, ELEM_BALANCE}, {ELEM_YIN, ELEM_YANG, ELEM_TAO},
    {ELEM_ORDER, ELEM_LOVE, ELEM_HARMONY},
    // Chakras
    {ELEM_LIFE, ELEM_EARTH, ELEM_ROOT}, {ELEM_LOVE, ELEM_LIFE, ELEM_HEART_CHAKRA},
    {ELEM_BREATH, ELEM_TRUTH, ELEM_THROAT}, {ELEM_MIND, ELEM_INTUITION, ELEM_THIRD_EYE},
    {ELEM_SPIRIT, ELEM_LIGHT, ELEM_CROWN}, {ELEM_CROWN, ELEM_ROOT, ELEM_KUNDALINI},
    // Cosmos
    {ELEM_LIGHT, ELEM_AETHER, ELEM_STAR}, {ELEM_STAR, ELEM_SHADOW, ELEM_MOON},
    {ELEM_STAR, ELEM_FIRE, ELEM_SUN}, {ELEM_STAR, ELEM_VOID, ELEM_COSMOS},
    {ELEM_STAR, ELEM_CHAOS, ELEM_NEBULA}, {ELEM_STAR, ELEM_STAR, ELEM_GALAXY},
    {ELEM_GALAXY, ELEM_LIGHT, ELEM_QUASAR}, {ELEM_STAR, ELEM_SPIN, ELEM_PULSAR},
    {ELEM_VOID, ELEM_COSMOS, ELEM_BLACK_HOLE}, {ELEM_LIGHT, ELEM_COSMOS, ELEM_WHITE_HOLE},
    {ELEM_BLACK_HOLE, ELEM_WHITE_HOLE, ELEM_SINGULARITY},
    {ELEM_AETHER, ELEM_MEMORY, ELEM_AKASHA},
    // Arcana
    {ELEM_SPIRIT, ELEM_VOID, ELEM_FOOL}, {ELEM_WILL, ELEM_MANA, ELEM_MAGICIAN},
    {ELEM_INTUITION, ELEM_MOON, ELEM_PRIESTESS}, {ELEM_LIFE, ELEM_LOVE, ELEM_EMPRESS},
    {ELEM_WILL, ELEM_ORDER, ELEM_EMPEROR}, {ELEM_WISDOM, ELEM_TRADITION, ELEM_HIEROPHANT},
    {ELEM_LOVE, ELEM_LOVE, ELEM_LOVERS}, {ELEM_WILL, ELEM_WILL, ELEM_CHARIOT},
    {ELEM_COURAGE, ELEM_HEART, ELEM_STRENGTH}, {ELEM_WISDOM, ELEM_SOLITUDE, ELEM_HERMIT},
    {ELEM_KARMA, ELEM_TIME, ELEM_WHEEL}, {ELEM_BALANCE, ELEM_TRUTH, ELEM_JUSTICE},
    {ELEM_SACRIFICE, ELEM_WISDOM, ELEM_HANGED}, {ELEM_VOID, ELEM_LIFE, ELEM_DEATH},
    {ELEM_BALANCE, ELEM_HARMONY, ELEM_TEMPERANCE}, {ELEM_DESIRE, ELEM_SHADOW, ELEM_DEVIL},
    {ELEM_CHAOS, ELEM_LIGHTNING, ELEM_TOWER}, {ELEM_HOPE, ELEM_STAR, ELEM_STAR_TAROT},
    {ELEM_DREAM, ELEM_MOON, ELEM_MOON_TAROT}, {ELEM_JOY, ELEM_SUN, ELEM_SUN_TAROT},
    {ELEM_KARMA, ELEM_SPIRIT, ELEM_JUDGEMENT}, {ELEM_HARMONY, ELEM_COSMOS, ELEM_WORLD},
    // Opus
    {ELEM_CHAOS, ELEM_VOID, ELEM_PRIMA_MATERIA},
    {ELEM_PRIMA_MATERIA, ELEM_DEATH, ELEM_NIGREDO}, {ELEM_ASH, ELEM_SOUL, ELEM_NIGREDO},
    {ELEM_NIGREDO, ELEM_LIGHT, ELEM_ALBEDO}, {ELEM_NIGREDO, ELEM_MOON, ELEM_ALBEDO},
    {ELEM_ALBEDO, ELEM_SUN, ELEM_CITRINITAS}, {ELEM_ALBEDO, ELEM_GOLD, ELEM_CITRINITAS},
    {ELEM_CITRINITAS, ELEM_BLOOD, ELEM_RUBEDO}, {ELEM_CITRINITAS, ELEM_FIRE, ELEM_RUBEDO},
    {ELEM_CRYSTAL, ELEM_QUINTESSENCE, ELEM_LAPIS}, {ELEM_RUBEDO, ELEM_SPIRIT, ELEM_TINCTURE},
    {ELEM_AETHER, ELEM_AETHER, ELEM_QUINTESSENCE},
    // Mysterium
    {ELEM_RUBEDO, ELEM_METAL, ELEM_PHILOSOPHER}, {ELEM_RUBEDO, ELEM_SULFUR, ELEM_PHILOSOPHER},
    {ELEM_LAPIS, ELEM_TINCTURE, ELEM_PHILOSOPHER}, {ELEM_RUBEDO, ELEM_LIFE, ELEM_ELIXIR},
    {ELEM_QUINTESSENCE, ELEM_LIFE, ELEM_ELIXIR}, {ELEM_RUBEDO, ELEM_AETHER, ELEM_AZOTH},
    {ELEM_LIFE, ELEM_CLAY, ELEM_HOMUNCULUS}, {ELEM_STONE, ELEM_SPIRIT, ELEM_GOLEM},
    {ELEM_THOUGHT, ELEM_THOUGHT, ELEM_EGREGORE}, {ELEM_LIGHT, ELEM_COSMOS, ELEM_PLEROMA},
    {ELEM_SOUL, ELEM_COSMOS, ELEM_MONAD}, {ELEM_BALANCE, ELEM_COSMOS, ELEM_TAO},
    // Ouroboros
    {ELEM_PHILOSOPHER, ELEM_ELIXIR, ELEM_OUROBOROS},
    {ELEM_MONAD, ELEM_TAO, ELEM_OUROBOROS}, {ELEM_WORLD, ELEM_FOOL, ELEM_OUROBOROS},
    {ELEM_DEATH, ELEM_LIFE, ELEM_OUROBOROS}, {ELEM_CHAOS, ELEM_ORDER, ELEM_OUROBOROS},
    {ELEM_BLACK_HOLE, ELEM_WHITE_HOLE, ELEM_OUROBOROS},
    // Essentia
    {ELEM_FIRE, ELEM_SPARK, ELEM_HEAT}, {ELEM_AIR, ELEM_VIBRATION, ELEM_SOUND},
    {ELEM_WISDOM, ELEM_LIGHT, ELEM_TRUTH}, {ELEM_PRANA, ELEM_SPIRIT, ELEM_KUNDALINI},
    {ELEM_VOID, ELEM_CHAOS, ELEM_SPIN}, {ELEM_WISDOM, ELEM_MEMORY, ELEM_TRADITION},
    {ELEM_HEART, ELEM_WILL, ELEM_COURAGE}, {ELEM_MIND, ELEM_VOID, ELEM_SOLITUDE},
    {ELEM_LOVE, ELEM_DEATH, ELEM_SACRIFICE}, {ELEM_METAL, ELEM_SUN, ELEM_GOLD},
};

#define RECIPE_COUNT (sizeof(g_recipes) / sizeof(g_recipes[0]))

static ElementType FindRecipe(ElementType a, ElementType b) {
    for (size_t i = 0; i < RECIPE_COUNT; i++)
        if ((g_recipes[i].a == a && g_recipes[i].b == b) ||
            (g_recipes[i].a == b && g_recipes[i].b == a))
            return g_recipes[i].r;
    return ELEM_EMPTY;
}

static ElementType Upgrade(ElementType t) {
    ElementType c = FindRecipe(t, t);
    if (c != ELEM_EMPTY && c != t) return c;
    switch (t) {
        case ELEM_FIRE: return ELEM_SPARK; case ELEM_WATER: return ELEM_ICE;
        case ELEM_EARTH: return ELEM_STONE; case ELEM_AIR: return ELEM_VOID;
        case ELEM_PLANT: return ELEM_LIFE; case ELEM_LIFE: return ELEM_SPIRIT;
        case ELEM_MIND: return ELEM_SOUL; case ELEM_RUBEDO: return ELEM_PHILOSOPHER;
        default: break;
    }
    int tier = g_elem[t].tier;
    for (int i = t + 1; i < ELEM_COUNT; i++)
        if (g_elem[i].tier == tier + 1) return i;
    return t;
}

// ============================================================================
// MOTES - Subtle particles, like dust in light
// ============================================================================

#define MAX_MOTES 60

typedef struct {
    Vector2 p, v;
    Color c;
    float life, size;
} Mote;

static Mote g_motes[MAX_MOTES];
static int g_moteCount = 0;

static void EmitMote(float x, float y, Color c) {
    if (g_moteCount >= MAX_MOTES) return;
    Mote *m = &g_motes[g_moteCount++];
    m->p = (Vector2){x, y};
    float a = ((float)rand() / RAND_MAX) * 2 * PI;
    float s = 15 + ((float)rand() / RAND_MAX) * 25;
    m->v = (Vector2){cosf(a) * s, sinf(a) * s - 10};
    m->c = c;
    m->life = 0.6f + ((float)rand() / RAND_MAX) * 0.4f;
    m->size = 1.5f + ((float)rand() / RAND_MAX) * 1.5f;
}

static void UpdateMotes(float dt) {
    for (int i = g_moteCount - 1; i >= 0; i--) {
        Mote *m = &g_motes[i];
        m->p.x += m->v.x * dt;
        m->p.y += m->v.y * dt;
        m->v.y += 20 * dt;
        m->v.x *= 0.97f;
        m->life -= dt;
        if (m->life <= 0) g_motes[i] = g_motes[--g_moteCount];
    }
}

static void DrawMotes(void) {
    for (int i = 0; i < g_moteCount; i++) {
        Mote *m = &g_motes[i];
        float a = m->life / 1.0f;
        Color c = m->c;
        c.a = (unsigned char)(a * 180);
        DrawCircleV(m->p, m->size * a, c);
    }
}

// ============================================================================
// GRAVITY - The pull of transformation
// ============================================================================

typedef enum { GRAV_DOWN, GRAV_LEFT, GRAV_UP, GRAV_RIGHT } Gravity;

static void GravDelta(Gravity g, int *dx, int *dy) {
    *dx = *dy = 0;
    if (g == GRAV_DOWN) *dy = 1;
    else if (g == GRAV_UP) *dy = -1;
    else if (g == GRAV_LEFT) *dx = -1;
    else if (g == GRAV_RIGHT) *dx = 1;
}

// ============================================================================
// THE WORK
// ============================================================================

typedef enum { PLAYING, OVER, COMPLETE } State;
typedef enum { SCREEN_GAME, SCREEN_BESTIARY } ScreenMode;
typedef enum { TAB_GUIDE, TAB_ELEMENTS } BestiaryTab;

// Tier names for display
static const char *g_tierNames[] = {
    "Prima Materia", "First Works", "Materia", "Vita", "Anima",
    "Spiritus", "Chakras", "Cosmos", "Arcana", "Opus", "Mysterium"
};

static struct {
    ElementType grid[GRID_HEIGHT][GRID_WIDTH];
    float glow[GRID_HEIGHT][GRID_WIDTH];     // Awareness glow
    float scale[GRID_HEIGHT][GRID_WIDTH];    // Subtle breathing

    ElementType falling, next;
    int fx;
    float fy, speed;

    int score, level, found;
    bool seen[ELEM_COUNT];
    State state;

    Gravity grav;
    float gravAngle, gravTarget;
    float rotCooldown;

    float time, msgTime;
    char msg[64];
    bool paused;

    // Bestiary state
    ScreenMode screen;
    BestiaryTab bestiaryTab;
    int bestiaryScroll;      // Current scroll position
    int bestiarySelected;    // Selected element index (in discovered list)
    float bestiaryAnim;      // Transition animation
} G;

static int W = 800, H = 480;
static int GX, GY;

// ============================================================================
// CORE MECHANICS
// ============================================================================

static bool AtEdge(int x, int y, int dx, int dy) {
    int nx = x + dx, ny = y + dy;
    if (nx < 0 || nx >= GRID_WIDTH || ny < 0 || ny >= GRID_HEIGHT) return true;
    return G.grid[ny][nx] != ELEM_EMPTY;
}

static bool Fall(void) {
    int dx, dy;
    GravDelta(G.grav, &dx, &dy);
    bool moved = false;

    int sx = (dx > 0) ? GRID_WIDTH - 2 : (dx < 0) ? 1 : 0;
    int ex = (dx > 0) ? -1 : GRID_WIDTH;
    int tx = (dx > 0) ? -1 : 1;
    int sy = (dy > 0) ? GRID_HEIGHT - 2 : (dy < 0) ? 1 : 0;
    int ey = (dy > 0) ? -1 : GRID_HEIGHT;
    int ty = (dy > 0) ? -1 : 1;

    for (int y = sy; y != ey; y += ty) {
        for (int x = sx; x != ex; x += tx) {
            if (G.grid[y][x] != ELEM_EMPTY) {
                int nx = x + dx, ny = y + dy;
                if (nx >= 0 && nx < GRID_WIDTH && ny >= 0 && ny < GRID_HEIGHT &&
                    G.grid[ny][nx] == ELEM_EMPTY) {
                    G.grid[ny][nx] = G.grid[y][x];
                    G.glow[ny][nx] = G.glow[y][x];
                    G.scale[ny][nx] = G.scale[y][x];
                    G.grid[y][x] = ELEM_EMPTY;
                    G.glow[y][x] = 0;
                    G.scale[y][x] = 1;
                    moved = true;
                }
            }
        }
    }
    return moved;
}

static int FloodCount(int x, int y, ElementType t, bool v[GRID_HEIGHT][GRID_WIDTH]) {
    if (x < 0 || x >= GRID_WIDTH || y < 0 || y >= GRID_HEIGHT) return 0;
    if (v[y][x] || G.grid[y][x] != t) return 0;
    v[y][x] = true;
    return 1 + FloodCount(x+1,y,t,v) + FloodCount(x-1,y,t,v) +
               FloodCount(x,y+1,t,v) + FloodCount(x,y-1,t,v);
}

static void FloodMark(int x, int y, ElementType t, bool m[GRID_HEIGHT][GRID_WIDTH]) {
    if (x < 0 || x >= GRID_WIDTH || y < 0 || y >= GRID_HEIGHT) return;
    if (m[y][x] || G.grid[y][x] != t) return;
    m[y][x] = true;
    FloodMark(x+1,y,t,m); FloodMark(x-1,y,t,m);
    FloodMark(x,y+1,t,m); FloodMark(x,y-1,t,m);
}

static bool Merge(void) {
    bool merged = false;
    bool done[GRID_HEIGHT][GRID_WIDTH] = {false};

    for (int y = 0; y < GRID_HEIGHT; y++) {
        for (int x = 0; x < GRID_WIDTH; x++) {
            ElementType t = G.grid[y][x];
            if (t == ELEM_EMPTY || done[y][x]) continue;

            bool temp[GRID_HEIGHT][GRID_WIDTH] = {false};
            int n = FloodCount(x, y, t, temp);

            if (n >= MERGE_THRESHOLD) {
                merged = true;
                bool mark[GRID_HEIGHT][GRID_WIDTH] = {false};
                FloodMark(x, y, t, mark);

                int sx = 0, sy = 0, c = 0;
                for (int cy = 0; cy < GRID_HEIGHT; cy++) {
                    for (int cx = 0; cx < GRID_WIDTH; cx++) {
                        if (mark[cy][cx]) {
                            done[cy][cx] = true;
                            sx += cx; sy += cy; c++;
                            float px = GX + cx * (CELL_SIZE + CELL_GAP) + CELL_SIZE / 2;
                            float py = GY + cy * (CELL_SIZE + CELL_GAP) + CELL_SIZE / 2;
                            for (int i = 0; i < 3; i++) EmitMote(px, py, g_elem[t].color);
                            G.grid[cy][cx] = ELEM_EMPTY;
                            G.glow[cy][cx] = 0;
                        }
                    }
                }

                G.score += g_elem[t].weight * n;

                ElementType up = Upgrade(t);
                int cx = sx / c, cy = sy / c;
                G.grid[cy][cx] = up;
                G.glow[cy][cx] = 1.0f;  // Awareness flash
                G.scale[cy][cx] = 0.5f;  // Start small, grow

                if (!G.seen[up]) {
                    G.seen[up] = true;
                    G.found++;
                    snprintf(G.msg, sizeof(G.msg), "%s", g_elem[up].name);
                    G.msgTime = 2.5f;
                }

                if (up == ELEM_OUROBOROS) {
                    G.state = COMPLETE;
                    snprintf(G.msg, sizeof(G.msg), "The Work is Complete");
                    G.msgTime = 10;
                }

                float px = GX + cx * (CELL_SIZE + CELL_GAP) + CELL_SIZE / 2;
                float py = GY + cy * (CELL_SIZE + CELL_GAP) + CELL_SIZE / 2;
                for (int i = 0; i < 5; i++) EmitMote(px, py, g_elem[up].color);
            }
        }
    }
    return merged;
}

static void Settle(void) {
    bool changed = true;
    int iter = 0;
    while (changed && iter++ < 30) changed = Fall() || Merge();
}

static void Rotate(int dir) {
    if (G.rotCooldown > 0) return;
    G.grav = (Gravity)((G.grav + dir + 4) % 4);
    G.gravTarget = G.grav * 90;
    while (G.gravTarget - G.gravAngle > 180) G.gravAngle += 360;
    while (G.gravAngle - G.gravTarget > 180) G.gravAngle -= 360;
    G.rotCooldown = 0.4f;
}

static ElementType RandElem(void) {
    if (G.level >= 3 && rand() % 100 < G.level * 2)
        return (ElementType)(5 + rand() % 8);
    return (ElementType)(1 + rand() % 4);
}

static void Spawn(void) {
    G.falling = G.next;
    G.next = RandElem();

    switch (G.grav) {
        case GRAV_DOWN:  G.fx = GRID_WIDTH / 2; G.fy = 0; break;
        case GRAV_UP:    G.fx = GRID_WIDTH / 2; G.fy = GRID_HEIGHT - 1; break;
        case GRAV_LEFT:  G.fx = GRID_WIDTH - 1; G.fy = GRID_HEIGHT / 2; break;
        case GRAV_RIGHT: G.fx = 0; G.fy = GRID_HEIGHT / 2; break;
    }

    if (G.grid[(int)G.fy][G.fx] != ELEM_EMPTY) {
        G.state = OVER;
        snprintf(G.msg, sizeof(G.msg), "The vessel overflows");
        G.msgTime = 3;
    }
}

static void Place(int x, int y, ElementType e) {
    if (x >= 0 && x < GRID_WIDTH && y >= 0 && y < GRID_HEIGHT) {
        G.grid[y][x] = e;
        G.glow[y][x] = 0.6f;
        G.scale[y][x] = 1.15f;
        if (!G.seen[e]) {
            G.seen[e] = true;
            G.found++;
        }
    }
}

static void Init(void) {
    memset(&G, 0, sizeof(G));
    G.level = 1;
    G.speed = 40;
    G.state = PLAYING;
    G.next = RandElem();
    G.grav = GRAV_DOWN;

    for (int y = 0; y < GRID_HEIGHT; y++)
        for (int x = 0; x < GRID_WIDTH; x++)
            G.scale[y][x] = 1;

    for (int i = 1; i <= 4; i++) { G.seen[i] = true; G.found++; }

    Spawn();

    int gw = GRID_WIDTH * CELL_SIZE + (GRID_WIDTH - 1) * CELL_GAP;
    int gh = GRID_HEIGHT * CELL_SIZE + (GRID_HEIGHT - 1) * CELL_GAP;
    GX = (W - gw) / 2;
    GY = (H - gh) / 2;
}

// ============================================================================
// PLUGIN
// ============================================================================

static void PluginInit(int w, int h) {
    W = w; H = h;
    srand((unsigned)time(NULL));
    Init();
}

// Count discovered elements
static int CountDiscovered(void) {
    int count = 0;
    for (int i = 1; i < ELEM_COUNT; i++) {
        if (G.seen[i]) count++;
    }
    return count;
}

// Get nth discovered element
static ElementType GetDiscoveredElement(int n) {
    int count = 0;
    for (int i = 1; i < ELEM_COUNT; i++) {
        if (G.seen[i]) {
            if (count == n) return (ElementType)i;
            count++;
        }
    }
    return ELEM_EMPTY;
}

// Bestiary update
static void BestiaryUpdate(const LlzInputState *in, float dt) {
    // Animate transition
    G.bestiaryAnim = Approach(G.bestiaryAnim, 1.0f, 6, dt);

    // Back button exits bestiary (on release)
    if (in->backReleased || in->button6Pressed) {
        G.screen = SCREEN_GAME;
        G.bestiaryAnim = 0;
        return;
    }

    // Switch tabs with button 3/4
    if (in->button3Pressed || in->swipeLeft) {
        G.bestiaryTab = TAB_GUIDE;
        G.bestiaryScroll = 0;
    }
    if (in->button4Pressed || in->swipeRight) {
        G.bestiaryTab = TAB_ELEMENTS;
        G.bestiaryScroll = 0;
    }

    // Scroll/navigate
    int delta = 0;
    if (in->scrollDelta != 0) delta = (in->scrollDelta > 0) ? 1 : -1;
    if (in->button1Pressed || in->upPressed) delta = -1;
    if (in->button2Pressed || in->downPressed) delta = 1;

    if (G.bestiaryTab == TAB_ELEMENTS) {
        int count = CountDiscovered();
        if (delta != 0 && count > 0) {
            G.bestiarySelected += delta;
            if (G.bestiarySelected < 0) G.bestiarySelected = count - 1;
            if (G.bestiarySelected >= count) G.bestiarySelected = 0;
        }
    } else {
        // Guide scroll
        if (delta != 0) {
            G.bestiaryScroll += delta;
            if (G.bestiaryScroll < 0) G.bestiaryScroll = 0;
            if (G.bestiaryScroll > 3) G.bestiaryScroll = 3;  // 4 pages of guide
        }
    }
}

static void PluginUpdate(const LlzInputState *in, float dt) {
    UpdateMotes(dt);
    G.time += dt;
    if (G.msgTime > 0) G.msgTime -= dt;
    if (G.rotCooldown > 0) G.rotCooldown -= dt;

    // Handle bestiary screen
    if (G.screen == SCREEN_BESTIARY) {
        BestiaryUpdate(in, dt);
        return;
    }

    // Button 6 opens bestiary (from game screen)
    if (in->button6Pressed) {
        G.screen = SCREEN_BESTIARY;
        G.bestiaryTab = TAB_GUIDE;
        G.bestiaryScroll = 0;
        G.bestiarySelected = 0;
        G.bestiaryAnim = 0;
        return;
    }

    // Smooth gravity angle
    G.gravAngle = Approach(G.gravAngle, G.gravTarget, 8, dt);

    // Breathe the cells
    for (int y = 0; y < GRID_HEIGHT; y++) {
        for (int x = 0; x < GRID_WIDTH; x++) {
            G.glow[y][x] = Approach(G.glow[y][x], 0, 3, dt);
            G.scale[y][x] = Approach(G.scale[y][x], 1, 6, dt);
        }
    }

    if (in->backReleased) {
        if (G.state != PLAYING) Init();
        else G.paused = !G.paused;
        return;
    }

    if (G.paused) {
        if (in->selectPressed || in->tap) G.paused = false;
        return;
    }

    if (G.state != PLAYING) {
        if (in->selectPressed || in->tap) Init();
        return;
    }

    // Rotate dimension
    if (in->button3Pressed || in->swipeLeft) Rotate(-1);
    if (in->button4Pressed || in->swipeRight) Rotate(1);

    // Move
    int mv = 0;
    if (in->scrollDelta != 0) mv = (in->scrollDelta > 0) ? 1 : -1;
    if (in->button1Pressed) mv = -1;
    if (in->button2Pressed) mv = 1;

    if (mv != 0) {
        if (G.grav == GRAV_DOWN || G.grav == GRAV_UP) {
            int nx = G.fx + mv;
            if (nx >= 0 && nx < GRID_WIDTH) G.fx = nx;
        } else {
            float ny = G.fy + mv;
            if (ny >= 0 && ny < GRID_HEIGHT) G.fy = ny;
        }
    }

    // Drop
    if (in->selectPressed || in->tap) {
        int dx, dy;
        GravDelta(G.grav, &dx, &dy);
        int lx = G.fx, ly = (int)G.fy;
        while (true) {
            int nx = lx + dx, ny = ly + dy;
            if (nx < 0 || nx >= GRID_WIDTH || ny < 0 || ny >= GRID_HEIGHT) break;
            if (G.grid[ny][nx] != ELEM_EMPTY) break;
            lx = nx; ly = ny;
        }
        Place(lx, ly, G.falling);
        Settle();
        Spawn();
        return;
    }

    // Natural fall
    int dx, dy;
    GravDelta(G.grav, &dx, &dy);
    float mult = in->button2Down ? 3 : 1;
    float amt = G.speed * mult * dt / CELL_SIZE;

    if (G.grav == GRAV_DOWN || G.grav == GRAV_UP) G.fy += dy * amt;
    else G.fx += (int)(dx * amt);

    int gx = G.fx, gy = (int)G.fy;
    gx = gx < 0 ? 0 : (gx >= GRID_WIDTH ? GRID_WIDTH - 1 : gx);
    gy = gy < 0 ? 0 : (gy >= GRID_HEIGHT ? GRID_HEIGHT - 1 : gy);

    if (AtEdge(gx, gy, dx, dy)) {
        Place(gx, gy, G.falling);
        Settle();
        Spawn();
    }

    // Level
    if (G.score > G.level * 120) {
        G.level++;
        G.speed += 4;
    }
}

// ============================================================================
// DRAWING - Quiet, warm, aware
// ============================================================================

// Draw the bestiary/guide screen
static void DrawBestiary(void) {
    float a = G.bestiaryAnim;

    // Background - warm parchment tone
    Color bg1 = {30, 27, 35, 255};
    Color bg2 = {40, 35, 45, 255};
    for (int i = 0; i < H; i++) {
        float t = (float)i / H;
        Color c = {
            (unsigned char)(bg1.r + t * (bg2.r - bg1.r)),
            (unsigned char)(bg1.g + t * (bg2.g - bg1.g)),
            (unsigned char)(bg1.b + t * (bg2.b - bg1.b)),
            (unsigned char)(255 * a)
        };
        DrawLine(0, i, W, i, c);
    }

    Color gold = {230, 200, 130, (unsigned char)(255 * a)};
    Color silver = {200, 195, 190, (unsigned char)(255 * a)};
    Color dim = {130, 120, 115, (unsigned char)(255 * a)};

    // Title
    const char *title = (G.bestiaryTab == TAB_GUIDE) ? "ALCHEMIST'S GUIDE" : "DISCOVERED ELEMENTS";
    int tw = MeasureText(title, 20);
    DrawText(title, (W - tw) / 2, 15, 20, gold);

    // Tab indicators
    Color tabGuide = (G.bestiaryTab == TAB_GUIDE) ? gold : dim;
    Color tabElem = (G.bestiaryTab == TAB_ELEMENTS) ? gold : dim;
    DrawText("< GUIDE", 20, 18, 12, tabGuide);
    DrawText("ELEMENTS >", W - 100, 18, 12, tabElem);

    // Content area
    int contentY = 50;
    int contentH = H - 90;

    if (G.bestiaryTab == TAB_GUIDE) {
        // How to play guide
        int page = G.bestiaryScroll;
        int lineY = contentY + 10;

        if (page == 0) {
            DrawText("THE GREAT WORK", (W - MeasureText("THE GREAT WORK", 16)) / 2, lineY, 16, gold);
            lineY += 35;

            DrawText("Match 3 or more of the same element to transmute", 40, lineY, 11, silver); lineY += 20;
            DrawText("them into something greater. Begin with Fire,", 40, lineY, 11, silver); lineY += 20;
            DrawText("Water, Earth, and Air - the prima materia.", 40, lineY, 11, silver); lineY += 35;

            DrawText("Your goal: Create the Ouroboros, the serpent", 40, lineY, 11, silver); lineY += 20;
            DrawText("that devours its own tail - symbol of eternal", 40, lineY, 11, silver); lineY += 20;
            DrawText("transformation and the completion of the Work.", 40, lineY, 11, silver); lineY += 35;

            DrawText("170 elements await discovery across 11 tiers.", 40, lineY, 11, dim); lineY += 20;

        } else if (page == 1) {
            DrawText("CONTROLS", (W - MeasureText("CONTROLS", 16)) / 2, lineY, 16, gold);
            lineY += 35;

            DrawText("SCROLL / BUTTON 1-2", 60, lineY, 11, gold);
            DrawText("Move falling piece", 260, lineY, 11, silver); lineY += 22;

            DrawText("TAP / SELECT", 60, lineY, 11, gold);
            DrawText("Drop piece instantly", 260, lineY, 11, silver); lineY += 22;

            DrawText("BUTTON 3-4 / SWIPE", 60, lineY, 11, gold);
            DrawText("Rotate gravity", 260, lineY, 11, silver); lineY += 22;

            DrawText("BUTTON 6", 60, lineY, 11, gold);
            DrawText("Open this guide", 260, lineY, 11, silver); lineY += 22;

            DrawText("BACK", 60, lineY, 11, gold);
            DrawText("Pause / Return", 260, lineY, 11, silver); lineY += 35;

            DrawText("Gravity rotation changes which way pieces fall.", 60, lineY, 11, dim);

        } else if (page == 2) {
            DrawText("TRANSFORMATION", (W - MeasureText("TRANSFORMATION", 16)) / 2, lineY, 16, gold);
            lineY += 35;

            DrawText("When 3+ identical elements connect, they merge", 40, lineY, 11, silver); lineY += 20;
            DrawText("into a higher form. Combinations follow ancient", 40, lineY, 11, silver); lineY += 20;
            DrawText("alchemical principles:", 40, lineY, 11, silver); lineY += 30;

            DrawText("Fire + Water = Steam", 60, lineY, 11, dim); lineY += 18;
            DrawText("Earth + Fire = Lava", 60, lineY, 11, dim); lineY += 18;
            DrawText("Plant + Rain = Life", 60, lineY, 11, dim); lineY += 18;
            DrawText("Life + Mind = Soul", 60, lineY, 11, dim); lineY += 30;

            DrawText("Higher tier elements appear as you level up.", 40, lineY, 11, silver);

        } else {
            DrawText("THE TIERS", (W - MeasureText("THE TIERS", 16)) / 2, lineY, 16, gold);
            lineY += 30;

            for (int i = 0; i <= 10; i++) {
                char tierLine[64];
                snprintf(tierLine, sizeof(tierLine), "%2d. %s", i, g_tierNames[i]);
                Color tierCol = (i < 3) ? silver : (i < 7) ? dim : gold;
                tierCol.a = (unsigned char)(tierCol.a * a);
                DrawText(tierLine, 60, lineY, 10, tierCol);
                lineY += 16;
            }

            lineY += 10;
            DrawText("From base matter to divine mystery.", 60, lineY, 10, dim);
        }

        // Page indicator
        char pageText[16];
        snprintf(pageText, sizeof(pageText), "%d / 4", page + 1);
        DrawText(pageText, (W - MeasureText(pageText, 12)) / 2, H - 35, 12, dim);

    } else {
        // Elements list
        int discovered = CountDiscovered();
        if (discovered == 0) {
            DrawText("No elements discovered yet.", (W - MeasureText("No elements discovered yet.", 14)) / 2, H / 2, 14, dim);
        } else {
            // Two-panel layout: list on left, detail on right
            int listW = 280;
            int detailX = listW + 40;

            // Draw element list (show 10 items, scroll)
            int visible = 10;
            int startIdx = G.bestiarySelected - visible / 2;
            if (startIdx < 0) startIdx = 0;
            if (startIdx + visible > discovered) startIdx = discovered - visible;
            if (startIdx < 0) startIdx = 0;

            for (int i = 0; i < visible && (startIdx + i) < discovered; i++) {
                int idx = startIdx + i;
                ElementType et = GetDiscoveredElement(idx);
                if (et == ELEM_EMPTY) continue;

                Element *e = &g_elem[et];
                int y = contentY + 10 + i * 32;
                bool selected = (idx == G.bestiarySelected);

                // Selection highlight
                if (selected) {
                    Color selBg = e->color;
                    selBg.a = (unsigned char)(40 * a);
                    DrawRectangleRounded((Rectangle){15, y - 4, listW - 10, 30}, 0.2f, 4, selBg);
                }

                // Element color swatch
                Color swatchCol = e->color;
                swatchCol.a = (unsigned char)(255 * a);
                DrawRectangleRounded((Rectangle){25, y, 24, 24}, 0.2f, 4, swatchCol);

                // Element name
                Color nameCol = selected ? gold : silver;
                nameCol.a = (unsigned char)(nameCol.a * a);
                DrawText(e->name, 60, y + 5, 12, nameCol);

                // Tier indicator
                char tierBuf[8];
                snprintf(tierBuf, sizeof(tierBuf), "T%d", e->tier);
                Color tierCol = dim;
                tierCol.a = (unsigned char)(tierCol.a * a);
                DrawText(tierBuf, listW - 40, y + 5, 10, tierCol);
            }

            // Scroll indicators
            if (startIdx > 0) {
                DrawText("^", listW / 2, contentY, 12, dim);
            }
            if (startIdx + visible < discovered) {
                DrawText("v", listW / 2, contentY + visible * 32 + 10, 12, dim);
            }

            // Detail panel for selected element
            ElementType selType = GetDiscoveredElement(G.bestiarySelected);
            if (selType != ELEM_EMPTY) {
                Element *sel = &g_elem[selType];

                // Large element swatch
                int swatchSize = 80;
                int swatchX = detailX + 100;
                int swatchY = contentY + 20;

                Color selColor = sel->color;
                selColor.a = (unsigned char)(255 * a);

                // Glow behind
                Color glowCol = selColor;
                glowCol.a = (unsigned char)(30 * a);
                DrawRectangleRounded((Rectangle){swatchX - 6, swatchY - 6, swatchSize + 12, swatchSize + 12}, 0.15f, 4, glowCol);

                DrawRectangleRounded((Rectangle){swatchX, swatchY, swatchSize, swatchSize}, 0.15f, 4, selColor);

                // Glyph on swatch
                Color glyphCol = (sel->color.r + sel->color.g + sel->color.b < 300)
                                 ? (Color){255, 255, 255, (unsigned char)(255 * a)}
                                 : (Color){40, 35, 45, (unsigned char)(255 * a)};
                int glyphW = MeasureText(sel->glyph, 24);
                DrawText(sel->glyph, swatchX + (swatchSize - glyphW) / 2, swatchY + (swatchSize - 24) / 2, 24, glyphCol);

                // Element name
                int nameY = swatchY + swatchSize + 15;
                DrawText(sel->name, detailX, nameY, 18, gold);

                // Tier and weight
                char infoLine[64];
                snprintf(infoLine, sizeof(infoLine), "Tier %d - %s", sel->tier, g_tierNames[sel->tier]);
                DrawText(infoLine, detailX, nameY + 25, 11, dim);

                if (sel->alive) {
                    DrawText("Living essence", detailX, nameY + 40, 10, (Color){160, 200, 140, (unsigned char)(180 * a)});
                }

                // Description
                int descY = nameY + 60;
                Color descCol = silver;
                descCol.a = (unsigned char)(descCol.a * a);

                // Word wrap description (simple split on ~45 chars)
                const char *desc = sel->desc;
                char line1[64] = {0}, line2[64] = {0};
                if (strlen(desc) <= 45) {
                    DrawText(desc, detailX, descY, 11, descCol);
                } else {
                    // Find a good break point
                    int breakPt = 45;
                    while (breakPt > 0 && desc[breakPt] != ' ') breakPt--;
                    if (breakPt == 0) breakPt = 45;
                    strncpy(line1, desc, breakPt);
                    line1[breakPt] = '\0';
                    strncpy(line2, desc + breakPt + 1, sizeof(line2) - 1);
                    DrawText(line1, detailX, descY, 11, descCol);
                    DrawText(line2, detailX, descY + 16, 11, descCol);
                }

                // Score value
                char scoreLine[32];
                snprintf(scoreLine, sizeof(scoreLine), "Value: %d pts", sel->weight);
                DrawText(scoreLine, detailX, descY + 40, 10, dim);
            }

            // Count display
            char countBuf[32];
            snprintf(countBuf, sizeof(countBuf), "%d / %d discovered", discovered, ELEM_COUNT - 1);
            DrawText(countBuf, (W - MeasureText(countBuf, 11)) / 2, H - 35, 11, dim);
        }
    }

    // Navigation hint
    const char *hint = "Scroll to navigate - Button 3/4 switch tabs - Back to return";
    int hintW = MeasureText(hint, 9);
    DrawText(hint, (W - hintW) / 2, H - 18, 9, (Color){100, 95, 90, (unsigned char)(150 * a)});
}

static void DrawCell(float cx, float cy, ElementType t, float glow, float scale) {
    if (t == ELEM_EMPTY || t >= ELEM_COUNT) return;
    Element *e = &g_elem[t];

    float s = CELL_SIZE * scale;
    float x = cx - s / 2, y = cy - s / 2;

    // The element becomes aware of itself through glow
    if (glow > 0.01f || e->tier >= 8) {
        float g = glow + (e->tier >= 8 ? 0.15f : 0);
        Color gc = e->color;
        gc.a = (unsigned char)(g * 60);
        DrawRectangleRounded((Rectangle){x - 3, y - 3, s + 6, s + 6}, 0.2f, 4, gc);
    }

    // Living elements breathe
    if (e->alive) {
        float breath = 1 + 0.02f * sinf(G.time * 2 + cx * 0.1f);
        s *= breath;
        x = cx - s / 2;
        y = cy - s / 2;
    }

    DrawRectangleRounded((Rectangle){x, y, s, s}, 0.18f, 4, e->color);

    // Subtle highlight - like light catching gold
    Color hi = {255, 255, 255, 25};
    DrawRectangleRounded((Rectangle){x + 2, y + 2, s - 4, s * 0.3f}, 0.2f, 4, hi);

    // Glyph
    Color tc = (e->color.r + e->color.g + e->color.b < 300)
               ? (Color){255, 255, 255, 255} : (Color){40, 35, 45, 255};
    int fs = 12;
    int tw = MeasureText(e->glyph, fs);
    DrawText(e->glyph, (int)(cx - tw / 2), (int)(cy - fs / 2), fs, tc);
}

static void PluginDraw(void) {
    // Bestiary screen
    if (G.screen == SCREEN_BESTIARY) {
        DrawBestiary();
        return;
    }

    // Warm, deep background
    Color bg1 = {25, 22, 30, 255};
    Color bg2 = {35, 30, 40, 255};
    for (int i = 0; i < H; i++) {
        float t = (float)i / H;
        Color c = {
            (unsigned char)(bg1.r + t * (bg2.r - bg1.r)),
            (unsigned char)(bg1.g + t * (bg2.g - bg1.g)),
            (unsigned char)(bg1.b + t * (bg2.b - bg1.b)),
            255
        };
        DrawLine(0, i, W, i, c);
    }

    // Vessel
    int pw = GRID_WIDTH * CELL_SIZE + (GRID_WIDTH - 1) * CELL_GAP + 16;
    int ph = GRID_HEIGHT * CELL_SIZE + (GRID_HEIGHT - 1) * CELL_GAP + 16;
    float px = GX - 8, py = GY - 8;

    // Subtle golden border that breathes
    float borderGlow = 0.4f + 0.1f * sinf(G.time * 0.8f);
    Color border = {180, 160, 120, (unsigned char)(borderGlow * 60)};
    DrawRectangleRounded((Rectangle){px - 2, py - 2, pw + 4, ph + 4}, 0.03f, 4, border);

    DrawRectangleRounded((Rectangle){px, py, pw, ph}, 0.03f, 4, (Color){20, 18, 25, 250});

    // Grid
    for (int y = 0; y < GRID_HEIGHT; y++) {
        for (int x = 0; x < GRID_WIDTH; x++) {
            float cx = GX + x * (CELL_SIZE + CELL_GAP);
            float cy = GY + y * (CELL_SIZE + CELL_GAP);

            // Empty cell - barely visible
            DrawRectangleRounded((Rectangle){cx, cy, CELL_SIZE, CELL_SIZE},
                                 0.12f, 4, (Color){30, 27, 38, 255});

            if (G.grid[y][x] != ELEM_EMPTY) {
                DrawCell(cx + CELL_SIZE / 2, cy + CELL_SIZE / 2,
                        G.grid[y][x], G.glow[y][x], G.scale[y][x]);
            }
        }
    }

    // Falling piece
    if (G.state == PLAYING && G.falling != ELEM_EMPTY) {
        float fcx = GX + G.fx * (CELL_SIZE + CELL_GAP) + CELL_SIZE / 2;
        float fcy = GY + G.fy * (CELL_SIZE + CELL_GAP) + CELL_SIZE / 2;

        // Ghost - very subtle
        int dx, dy;
        GravDelta(G.grav, &dx, &dy);
        int lx = G.fx, ly = (int)G.fy;
        while (true) {
            int nx = lx + dx, ny = ly + dy;
            if (nx < 0 || nx >= GRID_WIDTH || ny < 0 || ny >= GRID_HEIGHT) break;
            if (G.grid[ny][nx] != ELEM_EMPTY) break;
            lx = nx; ly = ny;
        }
        float gx = GX + lx * (CELL_SIZE + CELL_GAP);
        float gy = GY + ly * (CELL_SIZE + CELL_GAP);
        Color ghost = g_elem[G.falling].color;
        ghost.a = 30;
        DrawRectangleRounded((Rectangle){gx, gy, CELL_SIZE, CELL_SIZE}, 0.12f, 4, ghost);

        DrawCell(fcx, fcy, G.falling, 0.3f, 1);
    }

    DrawMotes();

    // UI - minimal, warm
    Color gold = {230, 200, 130, 255};
    Color silver = {200, 195, 190, 255};
    Color dim = {130, 120, 115, 255};

    // Left - score, level
    DrawText("SCORE", 15, 50, 9, dim);
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", G.score);
    DrawText(buf, 15, 62, 18, gold);

    DrawText("LEVEL", 15, 90, 9, dim);
    snprintf(buf, sizeof(buf), "%d", G.level);
    DrawText(buf, 15, 102, 16, silver);

    snprintf(buf, sizeof(buf), "%d/%d", G.found, ELEM_COUNT - 1);
    DrawText(buf, 15, 130, 10, dim);

    // Right - next, gravity
    int rx = W - 85;
    DrawText("NEXT", rx + 18, 50, 9, dim);
    DrawRectangleRounded((Rectangle){rx + 5, 62, 55, 55}, 0.1f, 4, (Color){30, 27, 38, 255});
    DrawCell(rx + 32, 89, G.next, 0, 1);

    const char *gn[] = {"v", "<", "^", ">"};
    DrawText(gn[G.grav], rx + 25, 125, 14, dim);

    // Title
    const char *title = "CAULDRON CASCADE";
    int tw = MeasureText(title, 16);
    DrawText(title, (W - tw) / 2, 12, 16, silver);

    // Message - quiet appearance
    if (G.msgTime > 0) {
        float a = Ease(fminf(G.msgTime / 0.5f, 1));
        int mw = MeasureText(G.msg, 14);
        Color mc = gold;
        mc.a = (unsigned char)(a * 255);
        DrawText(G.msg, (W - mw) / 2, H - 40, 14, mc);
    }

    // Overlays
    if (G.paused) {
        DrawRectangle(0, 0, W, H, (Color){0, 0, 0, 150});
        DrawText("PAUSED", (W - MeasureText("PAUSED", 28)) / 2, H / 2 - 20, 28, silver);
    }

    if (G.state == OVER) {
        DrawRectangle(0, 0, W, H, (Color){30, 20, 25, 180});
        DrawText("The vessel overflows", (W - MeasureText("The vessel overflows", 20)) / 2, H / 2 - 30, 20, silver);
        snprintf(buf, sizeof(buf), "Score: %d", G.score);
        DrawText(buf, (W - MeasureText(buf, 14)) / 2, H / 2 + 5, 14, dim);
    }

    if (G.state == COMPLETE) {
        // Golden awareness spreading outward
        float pulse = 0.3f + 0.2f * sinf(G.time * 1.5f);
        DrawRectangle(0, 0, W, H, (Color){255, 215, 100, (unsigned char)(pulse * 30)});

        DrawText("The Work is Complete", (W - MeasureText("The Work is Complete", 24)) / 2, H / 2 - 40, 24, gold);
        DrawText("Gold recognizes itself", (W - MeasureText("Gold recognizes itself", 12)) / 2, H / 2, 12, silver);
        snprintf(buf, sizeof(buf), "%d elements discovered", G.found);
        DrawText(buf, (W - MeasureText(buf, 11)) / 2, H / 2 + 25, 11, dim);
    }
}

static void PluginShutdown(void) {}
static bool PluginClose(void) { return false; }

static LlzPluginAPI g_plugin = {
    .name = "Cauldron Cascade",
    .description = "Gold becoming aware of itself becoming gold",
    .init = PluginInit,
    .update = PluginUpdate,
    .draw = PluginDraw,
    .shutdown = PluginShutdown,
    .wants_close = PluginClose,
    .category = LLZ_CATEGORY_GAMES
};

LlzPluginAPI* LlzGetPlugin(void) { return &g_plugin; }
