#include "rogue_encounter.h"
#include "rogue_state.h"

#include <melee/gm/gm_unsplit.h>
#include <melee/mn/types.h>
#include <melee/pl/forward.h>
#include <string.h>

static const CharacterKind enemies[] = {
    CKind_Mario, CKind_Fox, CKind_Captain, CKind_Donkey,
    CKind_Koopa, CKind_Link, CKind_Mars, CKind_Pikachu,
    CKind_Purin, CKind_Samus, CKind_Falco, CKind_Luigi,
    CKind_GameWatch, CKind_Kirby, CKind_Mewtwo, CKind_Ness,
    CKind_Peach, CKind_PopoNana, CKind_Yoshi, CKind_Zelda,
    CKind_Seak, CKind_CLink, CKind_DrMario, CKind_Emblem,
    CKind_Pichu, CKind_Ganon,
};

static const StKind stages[] = {
    St_Kind_Battle, St_Kind_Last, St_Kind_Story, St_Kind_Izumi,
};

void Rogue_GenerateEncounter(RogueEncounter* encounter, RogueRng* rng,
                             int floor)
{
    int i, variant;
    int act = (floor - 1) / ROGUE_FLOORS_PER_ACT + 1;
    int act_floor = (floor - 1) % ROGUE_FLOORS_PER_ACT + 1;
    int level = 3 + act + act_floor / 2;
    memset(encounter, 0, sizeof(*encounter));
    encounter->act = act;
    encounter->act_floor = act_floor;
    encounter->enemy_count = 1;
    encounter->type = ROGUE_ENCOUNTER_NORMAL;
    encounter->name = "Duel";
    encounter->enemy_kind =
        enemies[RogueRng_Bounded(rng, sizeof(enemies) / sizeof(enemies[0]))];
    encounter->stage =
        stages[RogueRng_Bounded(rng, sizeof(stages) / sizeof(stages[0]))];
    encounter->cpu_level = level > 9 ? 9 : level;
    encounter->stocks = 3;
    if (act_floor == 5) {
        encounter->type = ROGUE_ENCOUNTER_BOSS;
        encounter->cpu_level = act == 1 ? 8 : 9;
        encounter->stage = St_Kind_Last;
        encounter->enemy_kind = act == 1 ? CKind_Donkey : act == 2 ? CKind_Mario : CKind_MasterH;
        encounter->enemy_count = act == 2 ? 2 : 1;
        encounter->name = act == 1 ? "Giant Donkey Kong" : act == 2 ? "Metal Mario Brothers" : "Master Hand";
    } else if (act_floor == 3 || (act == 3 && act_floor == 4)) {
        encounter->type = ROGUE_ENCOUNTER_ELITE;
        encounter->cpu_level = level + 2 > 9 ? 9 : level + 2;
        encounter->name = "Elite";
    } else if (act_floor > 1) {
        variant = RogueRng_Bounded(rng, 3);
        if (variant) {
            encounter->type = variant == 1 ? ROGUE_ENCOUNTER_SWARM : ROGUE_ENCOUNTER_TEAM;
            encounter->enemy_count = variant == 1 ? 3 : 2;
            encounter->name = variant == 1 ? "Swarm" : "Tag Team";
        }
    }
    variant = RogueRng_Bounded(rng, 5);
    if (encounter->type == ROGUE_ENCOUNTER_ELITE) {
        static const char* names[] = { "Metal Mario & Luigi", "Giant Donkey Kong", "Giant Bowser", "Armored Samus", "Kirby Brigade" };
        static const CharacterKind kinds[] = { CKind_Mario, CKind_Donkey, CKind_Koopa, CKind_Samus, CKind_Kirby };
        encounter->name = names[variant];
        encounter->enemy_kind = kinds[variant];
        encounter->enemy_count = variant == 0 ? 2 : variant == 4 ? 3 : 1;
        encounter->stage = variant == 4 ? St_Kind_Story : St_Kind_Battle;
    }
    for (i = 0; i < encounter->enemy_count; ++i) {
        RogueEnemy* enemy = &encounter->enemies[i];
        enemy->kind = i == 0 || encounter->type == ROGUE_ENCOUNTER_SWARM ? encounter->enemy_kind :
            enemies[RogueRng_Bounded(rng, sizeof(enemies) / sizeof(enemies[0]))];
        enemy->cpu_level = encounter->cpu_level;
        enemy->stocks = 3;
        enemy->costume = i;
        enemy->attack_ratio = 1.0f + act * 0.08f;
        enemy->defense_ratio = 1.0f + act * 0.05f;
        enemy->model_scale = 1.0f;
        if (encounter->type == ROGUE_ENCOUNTER_SWARM) {
            enemy->stocks = 1;
            enemy->model_scale = 0.75f;
            enemy->attack_ratio = 0.85f;
        } else if (encounter->type == ROGUE_ENCOUNTER_TEAM) {
            enemy->stocks = 2;
        } else if (encounter->type == ROGUE_ENCOUNTER_ELITE) {
            enemy->kind = encounter->enemy_kind;
            enemy->attack_ratio += 0.12f;
            if (variant == 0) {
                enemy->kind = i == 0 ? CKind_Mario : CKind_Luigi;
                enemy->metal = true;
                enemy->stocks = 2;
            } else if (variant == 1 || variant == 2) {
                enemy->model_scale = 1.6f;
                enemy->stocks = 2;
            } else if (variant == 3) {
                enemy->metal = true;
                enemy->defense_ratio += 0.20f;
                enemy->stocks = 2;
            } else {
                enemy->model_scale = 0.8f;
                enemy->stocks = 2;
            }
        } else if (encounter->type == ROGUE_ENCOUNTER_BOSS) {
            enemy->attack_ratio += 0.15f;
            enemy->defense_ratio += 0.15f;
            if (act == 2) {
                enemy->kind = i == 0 ? CKind_Mario : CKind_Luigi;
                enemy->metal = true;
                enemy->stocks = 2;
            } else if (act == 3) {
                enemy->stocks = 1;
                enemy->model_scale = 1;
            } else {
                enemy->model_scale = 1.4f;
            }
        }
    }
}

void Rogue_SetupEncounter(VsModeData* data, const RogueEncounter* encounter,
                          CharacterKind player_kind)
{
    int i;
    memset(data, 0, sizeof(*data));
    gm_InitVsMode(data);
    data->start.rules.match_kind = MatchKind_Stock;
    data->start.rules.is_stock = true;
    data->start.rules.is_vs = true;
    data->start.rules.stkind = encounter->stage;
    data->start.rules.item_freq = -1;
    data->start.rules.x20 = 0;
    data->start.rules.sd_penalty = -1;
    /* Untimed: ties and timeouts must not trigger VS sudden death. */
    data->start.rules.timer_enabled = false;
    data->start.rules.is_teams = encounter->enemy_count > 1;
    for (i = 0; i < GM_MAX_PLAYERS; ++i) {
        data->start.players[i].slot = i;
    }
    data->start.players[0].ckind = player_kind;
    data->start.players[0].slot_type = Gm_PKind_Human;
    data->start.players[0].stocks = encounter->stocks;
    data->start.players[0].team = 0;
    for (i = 0; i < encounter->enemy_count && i < GM_MAX_PLAYERS - 1; ++i) {
        const RogueEnemy* enemy = &encounter->enemies[i];
        PlayerInitData* player = &data->start.players[i + 1];
        player->ckind = enemy->kind;
        player->slot_type = Gm_PKind_Cpu;
        player->stocks = enemy->stocks;
        player->cpu_level = enemy->cpu_level;
        player->attack_ratio = enemy->attack_ratio;
        player->defense_ratio = enemy->defense_ratio;
        player->model_scale = enemy->model_scale;
        player->vs_metal = enemy->metal;
        player->team = 1;
        player->color = player_kind == enemy->kind ? 1 : 0;
        if (enemy->kind == CKind_MasterH || enemy->kind == CKind_CrezyH) {
            player->stocks = 1;
            player->xC_b7 = true;
            player->hp = 300;
            player->xD_b2 = true;
            player->spawn_dir = -1;
            player->model_scale = 1;
            data->start.rules.x1_2 = true;
            data->start.rules.x1_3 = true;
            data->start.rules.x0_3 = 3;
        }
    }
}
