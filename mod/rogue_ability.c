#include "rogue_ability.h"
#include "rogue_hooks.h"
#include "rogue_state.h"
#include <melee/ef/efasync.h>
#include <melee/ef/eflib.h>
#include <melee/ft/ftdata.h>
#include <melee/ft/ftparts.h>
#include <melee/ft/inlines.h>
#include <melee/ft/kinds/ftCommon/forward.h>
#include <melee/ft/kinds/ftFox/types.h>
#include <melee/ft/kinds/ftFox/ftfoxspecialn.h>
#include <melee/ft/kinds/ftMario/ftmariospecials.h>
#include <melee/ft/kinds/ftSamus/inlines.h>
#include <melee/ft/kinds/ftMewtwo/ftmewtwospecialn.h>
#include <melee/ft/kinds/ftPeach/ftpeachspecialn.h>
#include <melee/ft/kinds/ftSeak/ftseakspecials.h>
#include <melee/pl/player.h>
#include <melee/it/it_26B1.h>
#include <stdio.h>
#include <string.h>

/* Host-owned lifetime, never appended to a disc-layout Fighter or shared data. */
typedef struct RogueFighterState {
    Fighter* fighter;
    const RogueAbilityDefinition* active;
    bool loaded[ROGUE_ABILITY_COUNT];
    bool loaded_sources[Ft_Kind_Max];
    void* native_attrs;
    struct Fighter_WaitAnimData* native_anims;
    u8 (*native_anim_flags)[2];
    u32 native_anim_count;
    union { double align; unsigned char bytes[0x424]; } attrs[Ft_Kind_Max];
    union Fighter_FighterVars native_vars;
    union Fighter_FighterVars source_vars[Ft_Kind_Max];
    HSD_GObjEvent native_death[3];
} RogueFighterState;
static RogueFighterState fighter_state;

bool Rogue_DebugGrantAbility(const char* key)
{
    int i;
    if (!Rogue_IsActive() || !key) return false;
    if (strcmp(key, "fox_reflector") == 0) key = "fox_down";
    if (strcmp(key, "falco_blaster") == 0) key = "falco_neutral";
    for (i = 1; i < ROGUE_ABILITY_COUNT; ++i) {
        const RogueAbilityDefinition* def = Rogue_GetAbility(i);
        if (def && strcmp(def->key, key) == 0) {
            g_rogue_run.ability[def->native_slot] = def->id;
            return true;
        }
    }
    return false;
}

void Rogue_AbilityFighterCreated(Fighter* fp)
{
    int i;
    if (!Rogue_IsRunPlayer(fp)) return;
    memset(&fighter_state, 0, sizeof(fighter_state));
    fighter_state.fighter = fp;
    for (i = 1; i < ROGUE_ABILITY_COUNT; ++i) {
        const RogueAbilityDefinition* def = Rogue_GetAbility(i);
        int slot, source;
        bool needed = false;
        if (!def) continue;
        source = def->internal_kind;
        for (slot = 0; slot < ROGUE_ABILITY_SLOTS; ++slot) {
            const RogueAbilityDefinition* equipped = Rogue_GetAbility(g_rogue_run.ability[slot]);
            if (!equipped) continue;
            if (equipped->internal_kind == source ||
                ((source == Ft_Kind_Zelda || source == Ft_Kind_Seak) &&
                 (equipped->internal_kind == Ft_Kind_Zelda || equipped->internal_kind == Ft_Kind_Seak)))
                needed = true;
        }
        if (!needed) continue;
        if (fighter_state.loaded_sources[source]) { fighter_state.loaded[i] = true; continue; }
        /* Fresh match heaps exist here; load dependencies before play starts.
         * Enqueueing asynchronous preloads from mode entry precedes heap setup. */
        ftData_8008572C(def->internal_kind);
        ftData_80085A14(def->internal_kind);
        if (ftData_UnkBytePerCharacter[source] != 255)
            efAsync_LoadSync(ftData_UnkBytePerCharacter[source]);
        if (def->attrs_size > sizeof(fighter_state.attrs[source])) continue;
        memcpy(fighter_state.attrs[source].bytes, gFtDataList[source]->ext_attr, def->attrs_size);
        {
            void** items = gFtDataList[def->internal_kind]->x48_items;
            void* attrs = fighter_state.attrs[source].bytes;
            switch (def->internal_kind) {
            case Ft_Kind_Koopa:
                it_8026B3F8(items[0], It_Kind_Koopa_Flame);
                break;
            case Ft_Kind_Samus:
                it_8026B3F8(items[0], It_Kind_Samus_Bomb);
                it_8026B3F8(items[1], It_Kind_Samus_Charge);
                it_8026B3F8(items[2], It_Kind_Samus_Missile);
                it_8026B3F8(items[3], It_Kind_Samus_GBeam);
                break;
            case Ft_Kind_Mewtwo:
                it_8026B3F8(items[0], It_Kind_Mewtwo_Disable);
                it_8026B3F8(items[1], It_Kind_Mewtwo_ShadowBall);
                break;
            case Ft_Kind_Ness:
                it_8026B3F8(items[0], It_Kind_Ness_PKFire);
                it_8026B3F8(items[1], It_Kind_Ness_PKFire_Flame);
                it_8026B3F8(items[2], It_Kind_Ness_PKFlush);
                it_8026B3F8(items[3], It_Kind_Ness_PKThunder);
                it_8026B3F8(items[4], It_Kind_Ness_PKThunder1);
                it_8026B3F8(items[5], It_Kind_Ness_PKThunder2);
                it_8026B3F8(items[6], It_Kind_Ness_PKThunder3);
                it_8026B3F8(items[7], It_Kind_Ness_PKThunder4);
                it_8026B3F8(items[8], It_Kind_Ness_PKFlush_Explode);
                it_8026B3F8(items[9], It_Kind_Ness_Bat);
                it_8026B3F8(items[10], It_Kind_Ness_Yoyo);
                break;
            case Ft_Kind_Peach:
                it_8026B3F8(items[0], It_Kind_Peach_Explode);
                it_8026B3F8(items[1], It_Kind_Peach_Turnip);
                it_8026B3F8(items[2], It_Kind_Peach_Parasol);
                it_8026B3F8(items[3], It_Kind_Peach_Toad);
                it_8026B3F8(items[4], It_Kind_Peach_ToadSpore);
                break;
            case Ft_Kind_Yoshi:
                it_8026B3F8(items[0], It_Kind_Yoshi_EggThrow);
                it_8026B3F8(items[1], It_Kind_Yoshi_Star);
                it_8026B3F8(items[2], It_Kind_Yoshi_EggLay);
                break;
            case Ft_Kind_Zelda:
                it_8026B3F8(items[0], It_Kind_Zelda_DinFire);
                it_8026B3F8(items[1], It_Kind_Zelda_DinFire_Explode);
                break;
            case Ft_Kind_Seak:
                it_8026B3F8(items[0], It_Kind_Seak_NeedleThrow);
                it_8026B3F8(items[1], It_Kind_Seak_NeedleHeld);
                it_8026B3F8(items[2], It_Kind_Seak_Vanish);
                it_8026B3F8(items[3], It_Kind_Seak_Chain);
                break;
            case Ft_Kind_GameWatch:
                it_8026B3F8(items[0], It_Kind_GameWatch_Greenhouse);
                it_8026B3F8(items[1], It_Kind_GameWatch_Manhole);
                it_8026B3F8(items[2], It_Kind_GameWatch_Fire);
                it_8026B3F8(items[3], It_Kind_GameWatch_Parachute);
                it_8026B3F8(items[4], It_Kind_GameWatch_Turtle);
                it_8026B3F8(items[5], It_Kind_GameWatch_Breath);
                it_8026B3F8(items[6], It_Kind_GameWatch_Judge);
                it_8026B3F8(items[7], It_Kind_GameWatch_Panic);
                it_8026B3F8(items[8], It_Kind_GameWatch_Chef);
                it_8026B3F8(items[9], It_Kind_GameWatch_Rescue);
                break;
            case Ft_Kind_Kirby:
                it_8026B3F8(items[0], It_Kind_Kirby_CBeam);
                it_8026B3F8(items[1], It_Kind_Kirby_Hammer);
                it_8026B3F8(items[2], It_Kind_Unk1);
                it_8026B3F8(items[3], It_Kind_Unk2);
                break;
            case Ft_Kind_Popo:
                it_8026B3F8(items[0], It_Kind_IceClimber_Ice);
                it_8026B3F8(items[1], It_Kind_IceClimber_Blizzard);
                it_8026B3F8(items[2], It_Kind_IceClimber_GumStrings);
                break;
            case Ft_Kind_Link: case Ft_Kind_CLink: {
                ftLk_DatAttrs* lk = attrs;
                it_8026B3F8(items[0], lk->x48);
                it_8026B3F8(items[1], lk->x2C);
                it_8026B3F8(items[2], lk->xBC);
                it_8026B3F8(items[3], lk->xC);
                it_8026B3F8(items[4], lk->x10);
                break;
            }
            case Ft_Kind_Mario:
                it_8026B3F8(items[0], It_Kind_Mario_Fire);
                it_8026B3F8(items[2], ((ftMario_DatAttrs*)attrs)->specials.cape_kind);
                break;
            case Ft_Kind_DrMario:
                it_8026B3F8(items[1], It_Kind_DrMario_Vitamin);
                it_8026B3F8(items[3], ((ftMario_DatAttrs*)attrs)->specials.cape_kind);
                break;
            case Ft_Kind_Luigi:
                it_8026B3F8(items[0], It_Kind_Luigi_Fire);
                break;
            case Ft_Kind_Pikachu: case Ft_Kind_Pichu: {
                ftPikachuAttributes* pk = attrs;
                it_8026B3F8(items[0], pk->xDC);
                it_8026B3F8(items[1], pk->specialn_itkind);
                it_8026B3F8(items[2], pk->specialairn_itkind);
                break;
            }
            default: break;
            }
        }
        if (def->internal_kind == Ft_Kind_Fox || def->internal_kind == Ft_Kind_Falco) {
            ftFox_DatAttrs* attrs = (ftFox_DatAttrs*) fighter_state.attrs[source].bytes;
            void** items = gFtDataList[def->internal_kind]->x48_items;
            it_8026B3F8(items[0], attrs->x1C_FOX_BLASTER_SHOT_ITKIND);
            it_8026B3F8(items[1], attrs->x20_FOX_BLASTER_GUN_ITKIND);
            it_8026B3F8(items[def->internal_kind == Ft_Kind_Fox ? 2 : 3],
                        def->internal_kind == Ft_Kind_Fox ? It_Kind_Fox_Illusion : It_Kind_Falco_Phantasm);
        }
        fighter_state.loaded[def->id] = true;
        fighter_state.loaded_sources[source] = true;
        if (source == Ft_Kind_Kirby) fighter_state.source_vars[source].kb.hat.kind = Ft_Kind_Kirby;
    }
}

bool Rogue_IsAbilityState(const Fighter* fp)
{
    return fp && fighter_state.fighter == fp && fighter_state.active != NULL;
}

void Rogue_AbilityCleanup(Fighter* fp)
{
    if (!Rogue_IsAbilityState(fp)) return;
    /* Remove attached articles before restoring the recipient's union. Their
     * destruction callbacks still require the source's attributes and vars. */
    switch (fighter_state.active->internal_kind) {
    case Ft_Kind_Samus:
        if (fighter_state.active->native_slot == ROGUE_ABILITY_NEUTRAL)
            ftSamus_UnkAndDestroyAllEF(fp->gobj);
        break;
    case Ft_Kind_Mewtwo:
        if (fighter_state.active->native_slot == ROGUE_ABILITY_NEUTRAL) {
            int charge = fp->u.mt.x2234_shadowBallCharge;
            ftMt_SpecialN_OnDeath(fp->gobj);
            fp->u.mt.x2234_shadowBallCharge = charge;
        }
        break;
    case Ft_Kind_Peach:
        if (fighter_state.active->native_slot == ROGUE_ABILITY_NEUTRAL)
            ftPe_SpecialN_OnDeath2(fp->gobj);
        break;
    case Ft_Kind_Seak:
        if (fighter_state.active->native_slot == ROGUE_ABILITY_SIDE)
            ftSk_SpecialS_CheckAndDestroyChain(fp->gobj);
        break;
    default: break;
    }
    if ((fighter_state.active->internal_kind == Ft_Kind_Mario ||
         fighter_state.active->internal_kind == Ft_Kind_DrMario) &&
        fighter_state.active->native_slot == ROGUE_ABILITY_SIDE)
        ftMr_SpecialS_RemoveCape(fp->gobj);
    if ((fighter_state.active->internal_kind == Ft_Kind_Fox ||
         fighter_state.active->internal_kind == Ft_Kind_Falco) &&
        fighter_state.active->native_slot == ROGUE_ABILITY_NEUTRAL)
        ftFx_SpecialN_RemoveBlaster(fp->gobj);
    fighter_state.source_vars[fighter_state.active->internal_kind] = fp->u;
    fp->u = fighter_state.native_vars;
    fp->death1_cb = fighter_state.native_death[0];
    fp->death2_cb = fighter_state.native_death[1];
    fp->death3_cb = fighter_state.native_death[2];
    fp->dat_attrs = fighter_state.native_attrs;
    fp->x24 = fighter_state.native_anims;
    fp->x28 = fighter_state.native_anim_flags;
    fp->x58C = fighter_state.native_anim_count;
    fp->reflecting = false;
    fp->reflect_hit_cb = NULL;
    /* The ordinary motion transition removes hitboxes and source effects. */
    fighter_state.active = NULL;
}

FighterKind Rogue_AbilitySourceKind(const Fighter* fp)
{
    return Rogue_IsAbilityState(fp) ? fighter_state.active->internal_kind : fp->kind;
}

ftData* Rogue_AbilityData(Fighter* fp)
{
    return Rogue_IsAbilityState(fp) ? gFtDataList[fighter_state.active->internal_kind] : fp->ft_data;
}

static FighterKind abilityFamily(FighterKind kind)
{
    switch (kind) {
    case Ft_Kind_Falco: return Ft_Kind_Fox;
    case Ft_Kind_DrMario: return Ft_Kind_Mario;
    case Ft_Kind_CLink: return Ft_Kind_Link;
    case Ft_Kind_Pichu: return Ft_Kind_Pikachu;
    case Ft_Kind_Ganon: return Ft_Kind_Captain;
    case Ft_Kind_Emblem: return Ft_Kind_Mars;
    case Ft_Kind_Nana: return Ft_Kind_Popo;
    default: return kind;
    }
}

union Fighter_FighterVars* Rogue_AbilityVars(Fighter* fp, FighterKind family)
{
    int source;
    if (fighter_state.fighter != fp) return &fp->u;
    family = abilityFamily(family);
    if (fighter_state.active &&
        abilityFamily(fighter_state.active->internal_kind) == family)
        return &fp->u;
    if (abilityFamily(fp->kind) == family)
        return fighter_state.active ? &fighter_state.native_vars : &fp->u;
    /* Projectiles can outlive the animation which created them. Their owner
     * callbacks must update the source's persistent state, never the unrelated
     * base fighter's overlapping union fields. Prefer the equipped clone. */
    for (source = 0; source < ROGUE_ABILITY_SLOTS; ++source) {
        const RogueAbilityDefinition* def = Rogue_GetAbility(g_rogue_run.ability[source]);
        if (def && abilityFamily(def->internal_kind) == family &&
            fighter_state.loaded_sources[def->internal_kind])
            return &fighter_state.source_vars[def->internal_kind];
    }
    for (source = 0; source < Ft_Kind_Max; ++source)
        if (abilityFamily(source) == family && fighter_state.loaded_sources[source])
            return &fighter_state.source_vars[source];
    return &fp->u;
}

bool Rogue_BorrowedTransform(Fighter_GObj* gobj, HSD_GObjEvent finish)
{
    Fighter* fp = GET_FIGHTER(gobj);
    const RogueAbilityDefinition* next;
    FighterKind old_kind, next_kind;
    int slot;
    if (!Rogue_IsAbilityState(fp)) return false;
    old_kind = fighter_state.active->internal_kind;
    if (old_kind != Ft_Kind_Zelda && old_kind != Ft_Kind_Seak) return false;
    next_kind = old_kind == Ft_Kind_Zelda ? Ft_Kind_Seak : Ft_Kind_Zelda;
    next = Rogue_GetAbility(1 + next_kind * 4 + ROGUE_ABILITY_DOWN);
    if (!next || !fighter_state.loaded_sources[next_kind]) return false;
    /* Transform the borrowed kit while retaining the player's base fighter. */
    for (slot = 0; slot < 4; ++slot) {
        const RogueAbilityDefinition* equipped = Rogue_GetAbility(g_rogue_run.ability[slot]);
        if (equipped && equipped->internal_kind == old_kind)
            g_rogue_run.ability[slot] = 1 + next_kind * 4 + slot;
    }
    fighter_state.source_vars[old_kind] = fp->u;
    fp->u = fighter_state.source_vars[next_kind];
    fighter_state.active = next;
    fp->dat_attrs = fighter_state.attrs[next_kind].bytes;
    fp->x24 = gFtDataList[next_kind]->xC;
    fp->x28 = gFtDataList[next_kind]->x10;
    fp->x58C = ftData_Table_Unk0[next_kind].count;
    finish(gobj);
    return true;
}

void Rogue_AbilityFighterDestroyed(Fighter* fp)
{
    if (fighter_state.fighter != fp) return;
    Rogue_AbilityCleanup(fp);
    memset(&fighter_state, 0, sizeof(fighter_state));
}

void Rogue_AbilityTransformed(Fighter* src, Fighter* dst)
{
    if (fighter_state.fighter != src || !Rogue_IsRunPlayer(dst)) return;
    Rogue_AbilityCleanup(src);
    /* Both native forms already exist; all borrowed assets and persistent
     * charge data belong to the run player and survive the entity swap. */
    fighter_state.fighter = dst;
}

Fighter_GObj* Rogue_AbilityClimberPartner(Fighter* fp)
{
    Fighter_GObj* partner = Player_GetEntityAtIndex(fp->player_id, 1);
    if (partner && GET_FIGHTER(partner)->kind != Ft_Kind_Nana) return NULL;
    return partner;
}

MotionState* Rogue_AbilityMotionState(Fighter* fp, int motion)
{
    const RogueAbilityDefinition* def;
    if (!Rogue_IsAbilityState(fp)) return NULL;
    def = fighter_state.active;
    if (!Rogue_IsRunPlayer(fp) || motion < def->first_state || motion > def->last_state) {
        Rogue_AbilityCleanup(fp);
        return NULL;
    }
    return &def->states[motion - ftCo_MS_Count];
}

int Rogue_AbilityMapBone(Fighter* fp, int bone)
{
    int mapped;
    if (!Rogue_IsAbilityState(fp)) return bone;
    mapped = ftPartsRemap(fp->kind, fighter_state.active->internal_kind, bone);
    /* Unmapped decorative bones cannot index outside the recipient skeleton. */
    if (mapped < 0 || mapped >= 0x8C || !fp->parts[mapped].joint)
        mapped = ftParts_GetBoneIndex(fp, FtPart_TransN);
    return mapped;
}

bool Rogue_TrySpecial(Fighter_GObj* gobj, RogueAbilitySlot slot, bool airborne)
{
    Fighter* fp = GET_FIGHTER(gobj);
    const RogueAbilityDefinition* def;
    ftData* source;
    if (!Rogue_IsRunPlayer(fp) || slot < 0 || slot >= ROGUE_ABILITY_SLOTS) return false;
    def = Rogue_GetAbility(g_rogue_run.ability[slot]);
    if (!def) { Rogue_AbilityCleanup(fp); return false; }
    if (def->native_slot != slot || fighter_state.fighter != fp ||
        !fighter_state.loaded[def->id]) return false;
    Rogue_AbilityCleanup(fp);
    fighter_state.native_attrs = fp->dat_attrs;
    fighter_state.native_anims = fp->x24;
    fighter_state.native_anim_flags = fp->x28;
    fighter_state.native_anim_count = fp->x58C;
    fighter_state.native_vars = fp->u;
    fighter_state.native_death[0] = fp->death1_cb;
    fighter_state.native_death[1] = fp->death2_cb;
    fighter_state.native_death[2] = fp->death3_cb;
    fp->u = fighter_state.source_vars[def->internal_kind];
    fighter_state.active = def;
    source = gFtDataList[def->internal_kind];
    fp->dat_attrs = fighter_state.attrs[def->internal_kind].bytes;
    fp->x24 = source->xC;
    fp->x28 = source->x10;
    fp->x58C = ftData_Table_Unk0[def->internal_kind].count;
    /* Source animation flags already identify the source skeleton. Melee's
     * ftPartsRemap path retargets its FigaTree to the unchanged base fighter. */
    (airborne ? def->air_enter : def->ground_enter)(gobj);
    return true;
}
