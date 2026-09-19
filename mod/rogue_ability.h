#ifndef MELEE_ROGUE_ABILITY_H
#define MELEE_ROGUE_ABILITY_H
#include <melee/ft/forward.h>
#include <melee/gm/forward.h>

typedef enum RogueAbilitySlot {
    ROGUE_ABILITY_NEUTRAL, ROGUE_ABILITY_SIDE,
    ROGUE_ABILITY_UP, ROGUE_ABILITY_DOWN, ROGUE_ABILITY_SLOTS
} RogueAbilitySlot;

typedef enum RogueAerialSlot {
    ROGUE_AERIAL_NAIR,
    ROGUE_AERIAL_FAIR,
    ROGUE_AERIAL_BAIR,
    ROGUE_AERIAL_UAIR,
    ROGUE_AERIAL_DAIR,
    ROGUE_AERIAL_SLOTS
} RogueAerialSlot;

#define ROGUE_AERIAL_PRICE 0
typedef enum RogueAbilityID {
    ROGUE_ABILITY_NATIVE,
    ROGUE_ABILITY_FOX_REFLECTOR = 1 + Ft_Kind_Fox * 4 + ROGUE_ABILITY_DOWN,
    ROGUE_ABILITY_COUNT = 1 + Ft_Kind_Max * 4
} RogueAbilityID;
typedef enum RogueAbilityCompatibility {
    ROGUE_COMPAT_NATIVE, ROGUE_COMPAT_SIMPLE,
    ROGUE_COMPAT_ADAPTED, ROGUE_COMPAT_UNSUPPORTED
} RogueAbilityCompatibility;
enum RogueAbilityFlags {
    ROGUE_ABILITY_NEEDS_ARTICLE = 1 << 0,
    ROGUE_ABILITY_NEEDS_ATTRS = 1 << 1,
    ROGUE_ABILITY_NEEDS_ANIMATION = 1 << 2,
    ROGUE_ABILITY_NEEDS_BONE_MAP = 1 << 3,
    ROGUE_ABILITY_NEEDS_STATE_TABLE = 1 << 4
};
typedef struct RogueAbilityDefinition {
    RogueAbilityID id;
    const char* key;
    const char* name;
    CharacterKind source_kind;
    FighterKind internal_kind;
    RogueAbilitySlot native_slot;
    HSD_GObjEvent ground_enter, air_enter;
    RogueAbilityCompatibility compatibility;
    unsigned flags;
    int first_state, last_state;
    MotionState* states;
    /* Runtime lifecycle coverage by internal FighterKind bit; this does not
     * certify every collision, visual effect or opponent matchup. */
    u32 tested_recipients;
    unsigned attrs_size;
} RogueAbilityDefinition;
const RogueAbilityDefinition* Rogue_GetAbility(RogueAbilityID id);
bool Rogue_DebugGrantAbility(const char* key);
void Rogue_AbilityFighterCreated(Fighter* fp);
void Rogue_AbilityFighterDestroyed(Fighter* fp);
void Rogue_AbilityTransformed(Fighter* src, Fighter* dst);
Fighter_GObj* Rogue_AbilityClimberPartner(Fighter* fp);
void Rogue_AbilityCleanup(Fighter* fp);
MotionState* Rogue_AbilityMotionState(Fighter* fp, int motion);
bool Rogue_TrySpecial(Fighter_GObj* gobj, RogueAbilitySlot slot, bool airborne);
int Rogue_AbilityMapBone(Fighter* fp, int bone);
bool Rogue_IsAbilityState(const Fighter* fp);
FighterKind Rogue_AbilitySourceKind(const Fighter* fp);
ftData* Rogue_AbilityData(Fighter* fp);
union Fighter_FighterVars;
union Fighter_FighterVars* Rogue_AbilityVars(Fighter* fp, FighterKind family);
bool Rogue_BorrowedTransform(Fighter_GObj* gobj, HSD_GObjEvent finish);
RogueAbilityID Rogue_AbilityForOpponent(CharacterKind opponent, RogueAbilitySlot slot);
FighterKind Rogue_InternalKindForCharacter(CharacterKind character);

const char* Rogue_AerialSlotName(RogueAerialSlot slot);
CharacterKind Rogue_AerialSource(RogueAerialSlot slot);
bool Rogue_BuyAerial(RogueAerialSlot slot, CharacterKind source);
bool Rogue_AerialTryEnter(Fighter_GObj* gobj, FtMotionId msid);
float Rogue_AerialLandingLag(Fighter* fp, FtMotionId msid, float native_lag);
#endif
