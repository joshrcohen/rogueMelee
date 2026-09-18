#ifndef MELEE_ROGUE_PROGRESSION_H
#define MELEE_ROGUE_PROGRESSION_H

#include <Runtime/platform.h>
#include <melee/gm/types.h>

#define ROGUE_STATE_PROGRESSION 7

/*
 * Exact 0x20-byte payload consumed by Melee's retail GS_INTRO_EASY scene.
 * During progression, the native "allies" side is route choice LEFT and the
 * native "enemies" side is route choice RIGHT.
 */
typedef struct RogueProgressionIntroData {
    s32 model_scale_kind;
    s32 game_type;
    u8 port;
    u8 nametag;
    u8 stage_number;
    u8 ally_count;
    u8 enemy_count;
    u8 allies[3];
    u8 enemies[3];
    u8 ally_costumes[3];
    u8 enemy_costumes[3];
    u8 ally_flags[3];
    u8 enemy_flags[3];
    u8 pad;
} RogueProgressionIntroData;

extern RogueProgressionIntroData g_rogue_progression_intro;

void RogueProgression_Enter(GameModeState* state);
void RogueProgression_Exit(GameModeState* state);

/*
 * Called from the narrow gm_1832 postpatch. Returns true only while the
 * dedicated Rogue progression state owns GS_INTRO_EASY.
 */
bool Rogue_ProgressionIntroFrame(void);

#endif
