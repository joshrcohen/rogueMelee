#include "rogue_progression.h"

#include "rogue.h"
#include "rogue_format.h"
#include "rogue_state.h"

#include <melee/ft/forward.h>
#include <melee/gr/stage.h>
#include <melee/gm/forward.h>
#include <melee/gm/gm_1A3F.h>
#include <melee/gm/gmscene.h>
#include <melee/lb/lbaudio_ax.h>
#include <melee/lb/lbdvd.h>
#include <melee/lb/types.h>
#include <melee/mn/inlines.h>
#include <melee/mn/mnmain.h>
#include <sysdolphin/baselib/sislib.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

RogueProgressionIntroData g_rogue_progression_intro;

static HSD_Text* lines[64];
static unsigned line_count;

static bool ui_open;
static bool build_open;
static bool has_reward;
static bool upgrade_chosen;
static int upgrade_cursor;
static int upgrade_taken = -1;
static int fight_cursor;
static int fight_locked = -1;
static int confirm_timer;
static int gold_gain;
static int target_floor;
static int target_act_floor;

static GXColor ui_white = {237, 239, 250, 255};
static GXColor ui_gold = {255, 200, 0, 255};
static GXColor ui_muted = {175, 187, 215, 255};
static GXColor ui_dark = {9, 15, 35, 235};
static GXColor ui_black = {18, 21, 34, 245};
static GXColor ui_red = {198, 28, 20, 255};
static GXColor ui_panel_color = {18, 22, 38, 220};

static void ui_clear(void)
{
    unsigned i;

    for (i = 0; i < line_count; ++i) {
        if (lines[i] != NULL)
            HSD_SisLib_803A5CC4(lines[i]);
    }

    line_count = 0;
}

static void ui_encode(char* out, const char* in)
{
    unsigned n = 0;
    unsigned char lo;

    while (*in && n < 120) {
        lo = 0;

        switch (*in) {
        case '+': lo = 0x7B; break;
        case '%': lo = 0x93; break;
        case '/': lo = 0x5E; break;
        case '[': lo = 0x6D; break;
        case ']': lo = 0x6E; break;
        case '>': lo = 0x84; break;
        case '<': lo = 0x83; break;
        case '|': lo = 0x62; break;
        case '!': lo = 0x49; break;
        case '?': lo = 0x48; break;
        case '=': lo = 0x81; break;
        case '&': lo = 0x95; break;
        case ';': lo = 0x47; break;
        case '(': lo = 0x69; break;
        case ')': lo = 0x6A; break;
        }

        if (lo) {
            out[n++] = (char) 0x81;
            out[n++] = (char) lo;
        } else {
            out[n++] = *in;
        }

        ++in;
    }

    out[n] = 0;
}

static HSD_Text* ui_object(float x, float y, float size, GXColor color)
{
    HSD_Text* text;

    if (line_count >= sizeof(lines) / sizeof(lines[0]))
        return NULL;

    /*
     * GS_INTRO_EASY already owns SIS slot 0 / canvas 0. Attach only Rogue
     * text objects to that retail canvas; never initialize a second SIS scene.
     */
    text = HSD_SisLib_803A6754(0, 0);
    if (text == NULL)
        return NULL;

    lines[line_count++] = text;

    text->pos_x = 320.0f + x * 16.0f;
    text->pos_y = 240.0f + y * 16.0f;
    text->pos_z = 0.0f;
    text->font_size.x = size * 16.0f;
    text->font_size.y = size * 16.0f;
    text->default_kerning = 1;
    text->text_color = color;

    return text;
}

static void ui_at(float x, float y, float size, GXColor color,
                  const char* fmt, ...)
{
    char raw[256];
    char encoded[128];
    va_list ap;
    HSD_Text* text;

    va_start(ap, fmt);
    vsnprintf(raw, sizeof(raw), fmt, ap);
    va_end(ap);

    ui_encode(encoded, raw);

    text = ui_object(x, y, size, color);
    if (text != NULL)
        HSD_SisLib_803A6B98(text, 0.0f, 0.0f, "%s", encoded);
}

static HSD_Text* ui_panel(float x, float y, float width, float height,
                          GXColor bg, GXColor fg)
{
    HSD_Text* text = ui_object(x, y, .0100f, fg);

    if (text == NULL)
        return NULL;

    text->bg_color = bg;
    text->box_size_x = width / .0100f;
    text->box_size_y = height / .0100f;
    HSD_SisLib_803A6B98(text, 0.0f, 0.0f, " ");

    return text;
}

static void split_description(const char* text,
                              char* first, unsigned first_size,
                              char* second, unsigned second_size,
                              int width)
{
    int len;
    int cut;

    if (first_size)
        first[0] = 0;
    if (second_size)
        second[0] = 0;

    if (text == NULL || *text == 0)
        return;

    len = strlen(text);
    cut = len > width ? width : len;

    if (len > width) {
        while (cut > 0 && text[cut] != ' ')
            --cut;

        if (cut <= 0)
            cut = width;
    }

    if ((unsigned) cut >= first_size)
        cut = first_size - 1;

    memcpy(first, text, cut);
    first[cut] = 0;

    text += cut;
    while (*text == ' ')
        ++text;

    if (second_size) {
        strncpy(second, text, second_size - 1);
        second[second_size - 1] = 0;
    }
}

static const char* reward_category(const RogueReward* reward)
{
    switch (reward->type) {
    case ROGUE_REWARD_ABILITY:
        return "SPECIAL";

    case ROGUE_REWARD_STATIC:
    case ROGUE_REWARD_COMBO_ENGINE:
    case ROGUE_REWARD_MOMENTUM:
    case ROGUE_REWARD_PARRY_HEAL:
    case ROGUE_REWARD_SECOND_SHELL:
    case ROGUE_REWARD_BLOODLUST:
    case ROGUE_REWARD_HEAVY_ARMOR:
    case ROGUE_REWARD_RECOVERY_WINDOW:
    case ROGUE_REWARD_LAST_STAND:
    case ROGUE_REWARD_MIRROR:
    case ROGUE_REWARD_PIERCING_SHOTS:
    case ROGUE_REWARD_GLASS_CANNON:
        return "PASSIVE";

    case ROGUE_REWARD_DEFENSE:
    case ROGUE_REWARD_ANCHOR:
    case ROGUE_REWARD_BRACE:
        return "DEFENSE";

    case ROGUE_REWARD_MOVEMENT:
    case ROGUE_REWARD_AIR_CONTROL:
    case ROGUE_REWARD_JUMP_HEIGHT:
    case ROGUE_REWARD_EXTRA_JUMP:
    case ROGUE_REWARD_FAST_FALL:
    case ROGUE_REWARD_SLIDE:
        return "MOVEMENT";

    default:
        return "BOOST";
    }
}

static int encounter_gold(const RogueEncounter* encounter)
{
    return encounter->type == ROGUE_ENCOUNTER_BOSS ? 100 :
           encounter->type == ROGUE_ENCOUNTER_ELITE ? 60 : 30;
}

static void encounter_name(char* out, unsigned size,
                           const RogueEncounter* encounter)
{
    if (encounter->enemy_count <= 1) {
        snprintf(out, size, "%s",
                 RogueRoute_CharacterName(encounter->enemy_kind));
    } else if (encounter->enemy_count == 2) {
        snprintf(out, size, "%s + %s",
                 RogueRoute_CharacterName(encounter->enemies[0].kind),
                 RogueRoute_CharacterName(encounter->enemies[1].kind));
    } else {
        snprintf(out, size, "%s SQUAD",
                 RogueRoute_CharacterName(encounter->enemy_kind));
    }
}

static const char* ability_name(int slot)
{
    RogueAbilityID id = g_rogue_run.ability[slot];
    const RogueAbilityDefinition* def;

    if (id == ROGUE_ABILITY_NATIVE) {
        id = Rogue_AbilityForOpponent(g_rogue_run.player_kind, slot);
    }

    def = Rogue_GetAbility(id);
    return def ? def->name : "Native";
}

static void draw_node(float x, const char* label,
                      bool active, bool complete)
{
    HSD_Text* text;
    char encoded[128];
    GXColor bg = active ? ui_gold : complete ? ui_red : ui_panel_color;
    GXColor fg = active ? ui_black : ui_white;
    int entry;

    text = ui_panel(x, -13.05f, 4.15f, 1.55f, bg, fg);
    if (text == NULL)
        return;

    ui_encode(encoded, label);
    entry = HSD_SisLib_803A6B98(text, 13.0f, 11.0f, "%s", encoded);
    HSD_SisLib_803A7548(text, entry, 1.34f, 1.34f);
}

static void draw_reward_card(float x, int index, bool selected)
{
    RogueReward* reward = &g_rogue_run.current_rewards[index];
    HSD_Text* text;
    GXColor bg = selected ? ui_dark : ui_panel_color;
    GXColor fg = selected ? ui_gold : ui_white;
    GXColor sub = ui_muted;
    char encoded[128];
    char meta[80];
    char detail[180];
    char line1[64];
    char line2[64];
    int entry;

    Rogue_DescribeReward(reward, detail, sizeof(detail));
    split_description(detail,
                      line1, sizeof(line1),
                      line2, sizeof(line2),
                      31);

    snprintf(meta, sizeof(meta), "%s / %s",
             reward_category(reward),
             Rogue_RarityName(reward->rarity));

    text = ui_panel(x, -8.70f, 9.10f, 4.40f, bg, fg);
    if (text == NULL)
        return;

    ui_encode(encoded, reward->name);
    entry = HSD_SisLib_803A6B98(text, 18.0f, 14.0f, "%s", encoded);
    HSD_SisLib_803A7548(text, entry, 1.60f, 1.60f);

    ui_encode(encoded, meta);
    entry = HSD_SisLib_803A6B98(text, 18.0f, 50.0f, "%s", encoded);
    HSD_SisLib_803A7548(text, entry, .92f, .92f);
    HSD_SisLib_803A74F0(text, entry, &sub);

    ui_encode(encoded, line1);
    entry = HSD_SisLib_803A6B98(text, 18.0f, 80.0f, "%s", encoded);
    HSD_SisLib_803A7548(text, entry, .86f, .86f);

    if (line2[0]) {
        ui_encode(encoded, line2);
        entry = HSD_SisLib_803A6B98(text, 18.0f, 103.0f, "%s", encoded);
        HSD_SisLib_803A7548(text, entry, .86f, .86f);
    }
}

static void draw_fight_plate(float x, const RogueEncounter* encounter,
                             bool selected, bool locked)
{
    HSD_Text* text;
    GXColor bg = selected ? ui_gold : ui_panel_color;
    GXColor fg = selected ? ui_black : ui_white;
    GXColor sub = selected ? ui_black : ui_muted;
    char encoded[128];
    char opponent[96];
    char meta[120];
    char tag[120];
    int entry;

    encounter_name(opponent, sizeof(opponent), encounter);

    snprintf(meta, sizeof(meta), "%s / %s",
             RogueRoute_StageName(encounter->stage),
             RogueRoute_TypeName(encounter->type));

    if (locked) {
        snprintf(tag, sizeof(tag), "PICK UPGRADE FIRST");
    } else if (encounter->modifier && *encounter->modifier) {
        snprintf(tag, sizeof(tag), "%s", encounter->modifier);
    } else if (encounter->enemy_count > 0) {
        snprintf(tag, sizeof(tag), "%d STOCK%s",
                 encounter->enemies[0].stocks,
                 encounter->enemies[0].stocks == 1 ? "" : "S");
    } else {
        snprintf(tag, sizeof(tag), "STANDARD");
    }

    text = ui_panel(x, 4.55f, 14.15f, 3.45f, bg, fg);
    if (text == NULL)
        return;

    ui_encode(encoded, opponent);
    entry = HSD_SisLib_803A6B98(text, 20.0f, 13.0f, "%s", encoded);
    HSD_SisLib_803A7548(text, entry, 1.62f, 1.62f);

    ui_encode(encoded, meta);
    entry = HSD_SisLib_803A6B98(text, 20.0f, 52.0f, "%s", encoded);
    HSD_SisLib_803A7548(text, entry, .94f, .94f);
    HSD_SisLib_803A74F0(text, entry, &sub);

    ui_encode(encoded, tag);
    entry = HSD_SisLib_803A6B98(text, 20.0f, 80.0f, "%s", encoded);
    HSD_SisLib_803A7548(text, entry, .67f, .67f);
    HSD_SisLib_803A74F0(text, entry, &sub);
}

static void draw_bottom_bar(void)
{
    const RogueStats* stats = &g_rogue_run.stats;
    HSD_Text* text;
    char encoded[128];
    char line[190];
    int entry;
    int score =
        g_rogue_run.wins * 10000 +
        g_rogue_run.currency * 100;

    text = ui_panel(-20.0f, 8.70f, 40.0f, 5.45f,
                    ui_black, ui_white);
    if (text == NULL)
        return;

    ui_encode(encoded, "CURRENT CHARACTER BUILD / UPGRADES");
    entry = HSD_SisLib_803A6B98(text, 55.0f, 13.0f, "%s", encoded);
    HSD_SisLib_803A7548(text, entry, 1.68f, 1.68f);
    HSD_SisLib_803A74F0(text, entry, &ui_gold);

    snprintf(line, sizeof(line),
             "N %.11s   S %.11s   U %.11s   D %.11s",
             ability_name(ROGUE_ABILITY_NEUTRAL),
             ability_name(ROGUE_ABILITY_SIDE),
             ability_name(ROGUE_ABILITY_UP),
             ability_name(ROGUE_ABILITY_DOWN));
    ui_encode(encoded, line);
    entry = HSD_SisLib_803A6B98(text, 55.0f, 41.0f, "%s", encoded);
    HSD_SisLib_803A7548(text, entry, .91f, .91f);

    snprintf(line, sizeof(line),
             "DMG %.0f%%   DEF %.0f%%   RUN %.0f%%   SHIELD %.0f%%",
             stats->damage_dealt * 100.0f,
             stats->damage_received * 100.0f,
             stats->run_speed * 100.0f,
             stats->shield_health * 100.0f);
    ui_encode(encoded, line);
    entry = HSD_SisLib_803A6B98(text, 55.0f, 66.0f, "%s", encoded);
    HSD_SisLib_803A7548(text, entry, .87f, .87f);

    if (has_reward) {
        snprintf(line, sizeof(line),
                 "GOLD GAINED +%d   TOTAL GOLD %d   TOTAL SCORE %d",
                 gold_gain,
                 g_rogue_run.currency,
                 score);
    } else {
        snprintf(line, sizeof(line),
                 "TOTAL GOLD %d   TOTAL SCORE %d",
                 g_rogue_run.currency,
                 score);
    }

    ui_encode(encoded, line);
    entry = HSD_SisLib_803A6B98(text, 55.0f, 91.0f, "%s", encoded);
    HSD_SisLib_803A7548(text, entry, .87f, .87f);
}

static void draw_build(void)
{
    const RogueStats* stats = &g_rogue_run.stats;

    ui_clear();
    ui_panel(-18.2f, -12.5f, 36.4f, 24.0f, ui_dark, ui_white);

    ui_at(-15.8f, -10.6f, .026f, ui_gold,
          "CURRENT CHARACTER BUILD");

    ui_at(-15.0f, -7.7f, .017f, ui_white,
          "NEUTRAL: %s", ability_name(ROGUE_ABILITY_NEUTRAL));
    ui_at(-15.0f, -5.5f, .017f, ui_white,
          "SIDE: %s", ability_name(ROGUE_ABILITY_SIDE));
    ui_at(-15.0f, -3.3f, .017f, ui_white,
          "UP: %s", ability_name(ROGUE_ABILITY_UP));
    ui_at(-15.0f, -1.1f, .017f, ui_white,
          "DOWN: %s", ability_name(ROGUE_ABILITY_DOWN));

    ui_at(-15.0f, 2.2f, .016f, ui_white,
          "DAMAGE %.0f%%   DEFENSE %.0f%%",
          stats->damage_dealt * 100.0f,
          stats->damage_received * 100.0f);
    ui_at(-15.0f, 4.3f, .016f, ui_white,
          "RUN %.0f%%   SHIELD %.0f%%",
          stats->run_speed * 100.0f,
          stats->shield_health * 100.0f);
    ui_at(-15.0f, 6.4f, .016f, ui_white,
          "AIR +%.0f%%   JUMP +%.0f%%   EXTRA JUMPS +%d",
          stats->air_control_bonus * 100.0f,
          stats->jump_height_bonus * 100.0f,
          stats->extra_jumps);
    ui_at(-15.0f, 8.5f, .016f, ui_white,
          "KNOCKBACK +%.0f%%   RESIST %.0f%%",
          stats->knockback_dealt_bonus * 100.0f,
          stats->knockback_resistance * 100.0f);

    ui_at(-4.5f, 11.0f, .012f, ui_gold, "B: RETURN");
}

/*
 * READABILITY / REFERENCE PASS:
 * Keep native fighter models dominant while making overlay text
 * readable at normal game scale. Selected cards/plates stay dark
 * instead of covering the models with solid yellow blocks.
 */
static void draw_progression(void)
{
    const RogueRoute* route = &g_rogue_run.route;
    const RogueRouteRound* round = RogueRoute_Current(route);
    static const char* node_names[6] = {
        "CLEAR", "NEXT", "ELITE", "MATCH 4", "SHOP", "BOSS"
    };
    static const float node_x[6] = {
        -14.75f, -9.82f, -4.89f, .04f, 4.97f, 9.90f
    };
    int i;

    if (build_open) {
        draw_build();
        return;
    }

    ui_clear();

    /* Top strip. Native VS/demo fighters remain visible through the middle. */
    ui_panel(-20.0f, -15.0f, 40.0f, 5.75f, ui_black, ui_white);

    for (i = 0; i < 6; ++i) {
        bool complete = false;
        bool active = false;

        if (i < ROGUE_ROUTE_ROUNDS)
            complete = route->rounds[i].selected >= 0;

        if (target_act_floor < ROGUE_FLOORS_PER_ACT)
            active = i == target_act_floor - 1;
        else
            active = i == 4;

        draw_node(node_x[i], node_names[i], active, complete);
    }

    ui_at(-4.75f, -10.35f, .0175f, ui_white,
          "ACT %d   -   FLOOR %d",
          route->act, target_floor);

    if (has_reward && !upgrade_chosen) {
        ui_at(-4.55f, -9.22f, .0185f, ui_gold,
              "CHOOSE UPGRADE");

        for (i = 0; i < 3; ++i) {
            draw_reward_card(
                -14.75f + i * 9.80f,
                i,
                upgrade_cursor == i);
        }
    } else {
        ui_at(-5.30f, -7.85f, .0190f, ui_gold,
              has_reward ? "CHOOSE NEXT FIGHT" : "CHOOSE FIRST FIGHT");

        if (has_reward && upgrade_taken >= 0) {
            ui_at(-6.10f, -6.55f, .0115f, ui_muted,
                  "UPGRADE LOCKED: %s",
                  g_rogue_run.current_rewards[upgrade_taken].name);
        }
    }

    if (target_act_floor < ROGUE_FLOORS_PER_ACT &&
        round != NULL && round->generated)
    {
        for (i = 0; i < ROGUE_ROUTE_CHOICES; ++i) {
            bool selected =
                upgrade_chosen &&
                (fight_locked >= 0 ?
                     fight_locked == i :
                     fight_cursor == i);

            draw_fight_plate(
                i == 0 ? -14.75f : .55f,
                &round->choices[i],
                selected,
                !upgrade_chosen);
        }
    } else {
        HSD_Text* text;
        char encoded[128];
        char line[120];
        const char* boss =
            route->boss_generated ?
            RogueRoute_CharacterName(route->boss.enemy_kind) :
            "BOSS";

        text = ui_panel(-14.75f, 4.55f, 29.45f, 3.45f,
                        ui_panel_color,
                        upgrade_chosen ? ui_gold : ui_muted);

        if (text != NULL) {
            snprintf(line, sizeof(line),
                     "SHOP / REST AREA  ->  BOSS: %s",
                     boss);
            ui_encode(encoded, line);
            HSD_SisLib_803A6B98(text, 35.0f, 24.0f, "%s", encoded);

            ui_encode(encoded,
                      upgrade_chosen ?
                      "UPGRADE LOCKED IN - CONTINUING" :
                      "PICK AN UPGRADE FIRST");
            HSD_SisLib_803A6B98(text, 35.0f, 67.0f, "%s", encoded);
        }
    }

    draw_bottom_bar();

    if (has_reward && !upgrade_chosen) {
        ui_at(-8.30f, 13.05f, .0113f, ui_white,
              "LEFT / RIGHT: UPGRADE     A: SELECT     B: BUILD");
    } else if (target_act_floor >= ROGUE_FLOORS_PER_ACT) {
        ui_at(-7.10f, 13.05f, .0113f, ui_white,
              "UPGRADE SELECTED     CONTINUING TO SHOP / REST");
    } else if (confirm_timer > 0) {
        ui_at(-5.45f, 13.05f, .0113f, ui_gold,
              "MATCH SET - LOADING VS SCREEN");
    } else {
        ui_at(-8.00f, 13.05f, .0113f, ui_white,
              "LEFT / RIGHT: FIGHT     A: SELECT     B: BUILD");
    }
}

static int copy_encounter(const RogueEncounter* encounter,
                          u8* kinds, u8* costumes)
{
    int count;
    int i;

    if (encounter == NULL)
        return 0;

    count = encounter->enemy_count;
    if (count > 3)
        count = 3;

    for (i = 0; i < count; ++i) {
        kinds[i] = encounter->enemies[i].kind;
        costumes[i] = encounter->enemies[i].costume;
    }

    return count;
}

static void preload_intro(const RogueProgressionIntroData* intro)
{
    struct GameCache* cache =
        &lbDvd_GetPreloadCacheScene()->game_cache;
    u64 audio = 0;
    int count = 0;
    int i;

    /*
     * Follow retail Classic's preload order closely. First load one left-side
     * demo fighter, wait, then add the rest of both route-choice teams.
     */
    lbDvd_80018C6C();

    if (intro->ally_count > 0 &&
        intro->allies[0] != ChKind_None)
    {
        cache->entries[count].char_id = intro->allies[0];
        cache->entries[count].color = intro->ally_costumes[0];
        ++count;

        lbDvd_80018254();
        lbDvd_80018C2C(0xC7);
        lbDvd_80017700(4);
    }

    for (i = 1; i < intro->ally_count; ++i) {
        if (intro->allies[i] == ChKind_None)
            continue;

        cache->entries[count].char_id = intro->allies[i];
        cache->entries[count].color = intro->ally_costumes[i];
        ++count;
    }

    for (i = 0; i < intro->enemy_count; ++i) {
        if (intro->enemies[i] == ChKind_None)
            continue;

        cache->entries[count].char_id = intro->enemies[i];
        cache->entries[count].color = intro->enemy_costumes[i];
        ++count;
    }

    lbDvd_80018254();

    /*
     * GS_INTRO_EASY itself uses St_Kind_Dummy. Keep a harmless real-stage
     * cache entry. The selected encounter's real intro replaces it before the
     * actual match.
     */
    cache->stkind = St_Kind_Battle;
    lbDvd_80018254();

    for (i = 0; i < intro->ally_count; ++i) {
        if (intro->allies[i] != ChKind_None)
            audio |= lbAudioAx_80026E84(intro->allies[i]);
    }

    for (i = 0; i < intro->enemy_count; ++i) {
        if (intro->enemies[i] != ChKind_None)
            audio |= lbAudioAx_80026E84(intro->enemies[i]);
    }

    audio |= lbAudioAx_80026EBC(St_Kind_Battle);

    lbAudioAx_80026F2C(0x1C);
    lbAudioAx_8002702C(0xC, audio);
    lbAudioAx_80027168();
}

static int next_intro_state(void)
{
    const RogueEncounter* encounter = &g_rogue_run.current_encounter;
    int i;

    for (i = 0; i < encounter->enemy_count; ++i) {
        if (encounter->enemies[i].kind == CKind_MasterH ||
            encounter->enemies[i].kind == CKind_CrezyH)
        {
            return 5;
        }
    }

    return 1;
}

void RogueProgression_Enter(GameModeState* state)
{
    const RogueRouteRound* round;
    const RogueEncounter* left = NULL;
    const RogueEncounter* right = NULL;
    int i;

    (void) state;

    ui_clear();
    ui_open = false;
    build_open = false;
    confirm_timer = 0;
    fight_locked = -1;
    upgrade_taken = -1;

    has_reward = g_rogue_run.phase == ROGUE_PHASE_REWARD;
    upgrade_chosen = !has_reward;
    upgrade_cursor = 0;
    fight_cursor = 0;

    gold_gain =
        has_reward ? encounter_gold(&g_rogue_run.current_encounter) : 0;

    target_floor =
        has_reward ? g_rogue_run.floor + 1 : g_rogue_run.floor;
    target_act_floor =
        ((target_floor - 1) % ROGUE_FLOORS_PER_ACT) + 1;

    if (target_act_floor < ROGUE_FLOORS_PER_ACT) {
        if (!RogueRoute_Prepare(&g_rogue_run.route,
                                &g_rogue_run.route_rng,
                                target_floor))
        {
            return;
        }
    }

    round = RogueRoute_Current(&g_rogue_run.route);

    if (target_act_floor < ROGUE_FLOORS_PER_ACT &&
        round != NULL && round->generated)
    {
        left = &round->choices[0];
        right = &round->choices[1];
    }

    memset(&g_rogue_progression_intro, 0,
           sizeof(g_rogue_progression_intro));

    g_rogue_progression_intro.model_scale_kind = 0;
    g_rogue_progression_intro.game_type = 0;
    g_rogue_progression_intro.port = Rogue_ControllerPort();
    g_rogue_progression_intro.nametag = GM_NAMETAG_NONE;
    g_rogue_progression_intro.stage_number = (u8) target_floor;

    for (i = 0; i < 3; ++i) {
        g_rogue_progression_intro.allies[i] = ChKind_None;
        g_rogue_progression_intro.enemies[i] = ChKind_None;
    }

    if (left != NULL && right != NULL) {
        /*
         * Native Classic terminology calls these allies/enemies. For this
         * screen they are simply the LEFT and RIGHT route-choice teams.
         */
        g_rogue_progression_intro.ally_count =
            copy_encounter(left,
                           g_rogue_progression_intro.allies,
                           g_rogue_progression_intro.ally_costumes);

        g_rogue_progression_intro.enemy_count =
            copy_encounter(right,
                           g_rogue_progression_intro.enemies,
                           g_rogue_progression_intro.enemy_costumes);
    } else {
        /*
         * Boss floors do not branch. Show the player's real model on the left
         * and a retail-safe boss preview on the right when possible.
         *
         * Retail GS_INTRO_EASY explicitly aborts if one of its first enemy
         * kinds is Master Hand, so Master/Crazy Hand are never passed here.
         * The overlay still identifies the real boss.
         */
        g_rogue_progression_intro.ally_count = 1;
        g_rogue_progression_intro.allies[0] =
            g_rogue_run.player_kind;
        g_rogue_progression_intro.ally_costumes[0] =
            g_rogue_run.player_costume;

        if (g_rogue_run.route.boss_generated &&
            g_rogue_run.route.boss.enemy_kind != CKind_MasterH &&
            g_rogue_run.route.boss.enemy_kind != CKind_CrezyH)
        {
            g_rogue_progression_intro.enemy_count =
                copy_encounter(
                    &g_rogue_run.route.boss,
                    g_rogue_progression_intro.enemies,
                    g_rogue_progression_intro.enemy_costumes);
        }
    }

    if (g_rogue_progression_intro.ally_count <= 0) {
        g_rogue_progression_intro.ally_count = 1;
        g_rogue_progression_intro.allies[0] =
            g_rogue_run.player_kind;
        g_rogue_progression_intro.ally_costumes[0] =
            g_rogue_run.player_costume;
    }

    preload_intro(&g_rogue_progression_intro);
}

void RogueProgression_Exit(GameModeState* state)
{
    (void) state;

    ui_clear();
    ui_open = false;

    if (g_rogue_run.phase != ROGUE_PHASE_ENCOUNTER) {
        gm_ChangeGameModeAfterCurrentScene(GM_MENU);
        return;
    }

    if (Rogue_BeginCamp()) {
        gm_SetNextGameModeStateId(3);
        return;
    }

    gm_SetNextGameModeStateId(next_intro_state());
}

static void open_ui(void)
{
    ui_open = true;
    draw_progression();
}

bool Rogue_ProgressionIntroFrame(void)
{
    u32 input;

    if (gm_GetCurrentGameMode() != GM_ROGUE ||
        gm_GetCurrentSceneIndex() != ROGUE_STATE_PROGRESSION)
    {
        return false;
    }

    if (!ui_open)
        open_ui();

    if (confirm_timer > 0) {
        --confirm_timer;

        if (confirm_timer == 0) {
            /*
             * Match retail IntroEasy's own exit path: stop presentation audio
             * immediately before ending this GameScene.
             */
            lbAudioAx_800236DC();
            gm_801A4B60();
        }

        return true;
    }

    input = mn_80229624(Rogue_ControllerPort());

    if (input & MenuInput_Back) {
        build_open = !build_open;
        draw_progression();
        return true;
    }

    if (build_open)
        return true;

    if (has_reward && !upgrade_chosen) {
        if (input & (MenuInput_Left | MenuInput_Up)) {
            upgrade_cursor = (upgrade_cursor + 2) % 3;
            draw_progression();
        }

        if (input & (MenuInput_Right | MenuInput_Down)) {
            upgrade_cursor = (upgrade_cursor + 1) % 3;
            draw_progression();
        }

        if (input & (MenuInput_Confirm | MenuInput_StartButton)) {
            int chosen = upgrade_cursor;

            if (Rogue_SelectReward(chosen)) {
                upgrade_chosen = true;
                upgrade_taken = chosen;
                fight_cursor = 0;
                draw_progression();

                /*
                 * Boss floor: reward selection already selected the generated
                 * boss encounter. There is no left/right branch here.
                 */
                if (g_rogue_run.phase == ROGUE_PHASE_ENCOUNTER)
                    confirm_timer = 30;
            }
        }

        return true;
    }

    if (g_rogue_run.phase == ROGUE_PHASE_ROUTE) {
        if (input & (MenuInput_Left | MenuInput_Right |
                     MenuInput_Up | MenuInput_Down))
        {
            fight_cursor ^= 1;
            draw_progression();
        }

        if (input & (MenuInput_Confirm | MenuInput_StartButton)) {
            if (RogueRoute_Select(&g_rogue_run.route,
                                  fight_cursor,
                                  &g_rogue_run.current_encounter))
            {
                fight_locked = fight_cursor;
                g_rogue_run.phase = ROGUE_PHASE_ENCOUNTER;
                confirm_timer = 18;
                draw_progression();
            }
        }

        return true;
    }

    if (g_rogue_run.phase == ROGUE_PHASE_ENCOUNTER) {
        confirm_timer = 1;
        return true;
    }

    return true;
}
