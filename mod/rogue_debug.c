#include "rogue_debug.h"
#include "rogue_ui.h"

#if defined(ROGUE_QA) && ROGUE_QA == 3

#include "rogue_ability.h"
#include "rogue_state.h"
#include <dolphin/os.h>
#include <melee/ft/fighter.h>
#include <melee/ft/ftcommon.h>
#include <melee/ft/kinds/ftCommon/forward.h>
#include <melee/gm/gm_unsplit.h>
#include <melee/gr/stage.h>
#include <melee/pl/player.h>
#include <stdio.h>
#include <string.h>

/*
 * Host-driven ability compatibility harness.
 *
 * tools/qa/ability_matrix.py locates this mailbox in Dolphin's emulated RAM,
 * publishes one recipient/source/slot/ground-air case, and watches heartbeat
 * and status fields. If the game crashes or Dolphin halts, the host records
 * the exact active case before restarting and continuing the matrix.
 *
 * Keep the layout stable and 32-bit aligned: the Python host depends on it.
 */
enum {
    ROGUE_QA_WAITING = 0,
    ROGUE_QA_PREPARING = 1,
    ROGUE_QA_RUNNING = 2,
    ROGUE_QA_PASS = 3,
    ROGUE_QA_SOFT_FAIL = 4,
    ROGUE_QA_INVALID = 5,
    ROGUE_QA_NOT_APPLICABLE = 6,
};

volatile struct {
    char magic[16];
    u32 version;
    u32 command;
    u32 status;
    u32 heartbeat;
    s32 recipient;
    s32 source;
    s32 slot;
    s32 variant; /* 0 = ground, 1 = air */
    s32 error;
    u32 active_command;
    char note[64];
} rogue_qa_mailbox = { "ROGUE_QA_MATRIX1", 1 };

static bool reload_requested;
static bool invoked;
static u32 invoke_at;
static u32 finish_at;

static void note(const char* text)
{
    unsigned i = 0;
    if (!text) text = "";
    while (text[i] && i + 1 < sizeof(rogue_qa_mailbox.note)) {
        rogue_qa_mailbox.note[i] = text[i];
        ++i;
    }
    rogue_qa_mailbox.note[i] = 0;
}

static void note_case(const char* prefix, const RogueAbilityDefinition* def)
{
    char text[64];
    snprintf(text, sizeof(text), "%s %s %s",
             prefix ? prefix : "QA",
             rogue_qa_mailbox.variant ? "air" : "ground",
             def && def->name ? def->name : "unknown");
    note(text);
}

static void setup_safe_encounter(void)
{
    RogueEncounter* encounter = &g_rogue_run.current_encounter;
    RogueEnemy* enemy;

    memset(encounter, 0, sizeof(*encounter));
    encounter->type = ROGUE_ENCOUNTER_NORMAL;
    encounter->act = 1;
    encounter->act_floor = 1;
    encounter->name = "Ability QA";
    encounter->enemy_count = 1;
    encounter->enemy_kind =
        g_rogue_run.player_kind == CKind_Mario ? CKind_Fox : CKind_Mario;
    encounter->stage = St_Kind_Battle;
    encounter->cpu_level = 1;
    encounter->stocks = 9;

    enemy = &encounter->enemies[0];
    enemy->kind = encounter->enemy_kind;
    enemy->cpu_level = 1;
    enemy->stocks = 9;
    enemy->costume = 0;
    enemy->attack_ratio = 0.05f;
    enemy->defense_ratio = 1.0f;
    enemy->model_scale = 1.0f;
    enemy->metal = false;
}

static bool configure_case(void)
{
    CharacterKind recipient = (CharacterKind)rogue_qa_mailbox.recipient;
    CharacterKind source = (CharacterKind)rogue_qa_mailbox.source;
    int slot = rogue_qa_mailbox.slot;
    int variant = rogue_qa_mailbox.variant;
    RogueAbilityID ability;

    if (recipient < 0 || recipient >= CKind_Playable_Count ||
        source < 0 || source >= CKind_Playable_Count ||
        slot < 0 || slot >= ROGUE_ABILITY_SLOTS ||
        (variant != 0 && variant != 1)) {
        rogue_qa_mailbox.error = 1;
        note("invalid request");
        return false;
    }

    ability = Rogue_AbilityForOpponent(source, (RogueAbilitySlot)slot);
    if (ability == ROGUE_ABILITY_NATIVE || !Rogue_GetAbility(ability)) {
        rogue_qa_mailbox.error = 2;
        note("ability missing from registry");
        return false;
    }

    Rogue_NewRun(recipient, 0x51410000U |
                 ((u32)recipient << 8) | (u32)source);
    g_rogue_run.ability[slot] = ability;
    setup_safe_encounter();
    g_rogue_run.phase = ROGUE_PHASE_ENCOUNTER;
    return true;
}

CharacterKind Rogue_DebugTestFighter(void)
{
    return CKind_Mario;
}

bool Rogue_DebugMatrixEnabled(void)
{
    return true;
}

void Rogue_DebugMatrixModeLoad(void)
{
    reload_requested = false;
    invoked = false;
    invoke_at = finish_at = 0;
    rogue_qa_mailbox.status = ROGUE_QA_WAITING;
    rogue_qa_mailbox.heartbeat = 0;
    rogue_qa_mailbox.active_command = 0;
    rogue_qa_mailbox.error = 0;
    note("waiting for host");
    Rogue_NewRun(CKind_Mario, 0x5141524DU);
    setup_safe_encounter();
    g_rogue_run.phase = ROGUE_PHASE_ENCOUNTER;
}

bool Rogue_DebugMatrixConsumeReload(void)
{
    bool result = reload_requested;
    reload_requested = false;
    return result;
}

static bool invoke_test(Fighter_GObj* entity)
{
    Fighter* fp;
    const RogueAbilityDefinition* def;
    RogueAbilityID ability;
    int slot = rogue_qa_mailbox.slot;

    if (!entity || !entity->user_data) return false;
    fp = entity->user_data;
    ability = g_rogue_run.ability[slot];
    def = Rogue_GetAbility(ability);
    if (!def) return false;

    note_case("running", def);
    OSReport("ROGUE_QA command=%u recipient=%d source=%d slot=%d variant=%d ability=%s\n",
             rogue_qa_mailbox.active_command,
             rogue_qa_mailbox.recipient,
             rogue_qa_mailbox.source,
             slot,
             rogue_qa_mailbox.variant,
             def->name ? def->name : "unknown");

    if (rogue_qa_mailbox.variant) {
        /*
         * Exercise the source fighter's aerial entry point on the recipient.
         * The native helper clears the grounded state/ECB bookkeeping exactly
         * as ordinary aerial transitions do.
         */
        Rogue_AbilityCleanup(fp);
        ftCommon_8007D5D4(fp);
        fp->cur_pos.y += 18.0f;
        fp->self_vel.x = 0;
        fp->self_vel.y = 0;
    } else if (fp->ground_or_air != GA_Ground) {
        note("recipient was not grounded");
        return false;
    }

    return Rogue_TrySpecial(entity, (RogueAbilitySlot)slot,
                            rogue_qa_mailbox.variant != 0);
}

bool Rogue_DebugMatrixFrame(void)
{
    u32 frame = gm_GetFrameCount();
    u32 command = rogue_qa_mailbox.command;
    Fighter_GObj* entity;
    Fighter* fp;
    const RogueAbilityDefinition* def;

    ++rogue_qa_mailbox.heartbeat;

    /*
     * The host publishes request fields first and command last. A new command
     * rebuilds the fighter scene so source DAT/animation/article dependencies
     * are loaded from a clean heap before the move is invoked.
     */
    if (command && command != rogue_qa_mailbox.active_command) {
        rogue_qa_mailbox.active_command = command;
        rogue_qa_mailbox.error = 0;
        invoked = false;
        invoke_at = finish_at = 0;

        if (!configure_case()) {
            rogue_qa_mailbox.status = ROGUE_QA_INVALID;
            return false;
        }

        rogue_qa_mailbox.status = ROGUE_QA_PREPARING;
        note("reloading test fighter");
        reload_requested = true;
        return true;
    }

    if (rogue_qa_mailbox.status == ROGUE_QA_PREPARING) {
        if (reload_requested || frame < 60) return false;
        /*
         * Wait for the human fighter to finish native spawn/entry and settle
         * into Wait. This is more robust than assuming a fixed Ready/Go
         * duration across Dolphin configurations.
         */
        entity = Player_GetEntity(0);
        fp = entity ? entity->user_data : NULL;
        if (!fp || fp->ground_or_air != GA_Ground ||
            fp->motion_id != ftCo_MS_Wait)
            return false;

        /*
         * The 15-frame armed window gives the host time to observe the exact
         * case before a move that might immediately crash the emulator.
         */
        def = Rogue_GetAbility(g_rogue_run.ability[rogue_qa_mailbox.slot]);
        if (!def ||
            (rogue_qa_mailbox.variant ? def->air_enter == NULL :
                                        def->ground_enter == NULL)) {
            rogue_qa_mailbox.status = ROGUE_QA_NOT_APPLICABLE;
            note(rogue_qa_mailbox.variant ?
                 "source has no aerial entry" : "source has no ground entry");
            return false;
        }
        note_case("armed", def);
        rogue_qa_mailbox.status = ROGUE_QA_RUNNING;
        invoke_at = frame + 15;
        return false;
    }

    if (rogue_qa_mailbox.status != ROGUE_QA_RUNNING)
        return false;

    entity = Player_GetEntity(0);
    fp = entity ? entity->user_data : NULL;
    if (!fp) return false;

    if (!invoked && frame >= invoke_at) {
        if (!invoke_test(entity)) {
            rogue_qa_mailbox.error = 3;
            rogue_qa_mailbox.status = ROGUE_QA_SOFT_FAIL;
            note("move refused or setup unavailable");
            return false;
        }
        invoked = true;
        finish_at = frame + 90;
        return false;
    }

    if (invoked && frame >= finish_at) {
        /*
         * Force a conventional transition if the borrowed move is still
         * active. This deliberately exercises its cleanup path; many subtle
         * cross-character crashes only appear when an article/attribute swap
         * is torn down.
         */
        if (Rogue_IsAbilityState(fp)) {
            Fighter_ChangeMotionState(
                entity,
                fp->ground_or_air == GA_Air ? ftCo_MS_Fall : ftCo_MS_Wait,
                Ft_MF_None, 0.0f, 1.0f, 0.0f, NULL);
        }

        def = Rogue_GetAbility(g_rogue_run.ability[rogue_qa_mailbox.slot]);
        note_case("pass", def);
        OSReport("ROGUE_QA PASS command=%u recipient=%d source=%d slot=%d variant=%d\n",
                 rogue_qa_mailbox.active_command,
                 rogue_qa_mailbox.recipient,
                 rogue_qa_mailbox.source,
                 rogue_qa_mailbox.slot,
                 rogue_qa_mailbox.variant);
        rogue_qa_mailbox.status = ROGUE_QA_PASS;
    }

    return false;
}

bool Rogue_DebugAbilityTestEnabled(void) { return false; }
bool Rogue_DebugYoshiTestEnabled(void) { return false; }
void Rogue_DebugAbilityTestFrame(void) {}
bool Rogue_DebugIntroTestEnabled(void) { return false; }
void Rogue_DebugIntroTestFrame(void) {}

#else

CharacterKind Rogue_DebugTestFighter(void) { return CKind_Mario; }
bool Rogue_DebugMatrixEnabled(void) { return false; }
void Rogue_DebugMatrixModeLoad(void) {}
bool Rogue_DebugMatrixFrame(void) { return false; }
bool Rogue_DebugMatrixConsumeReload(void) { return false; }
bool Rogue_DebugAbilityTestEnabled(void) { return false; }
bool Rogue_DebugYoshiTestEnabled(void) { return false; }
void Rogue_DebugAbilityTestFrame(void) {}
bool Rogue_DebugIntroTestEnabled(void) { return false; }
void Rogue_DebugIntroTestFrame(void) { RogueUI_IntroFrame(); }

#endif
