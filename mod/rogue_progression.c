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

static HSD_Text* lines[96];
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

static GXColor ui_white = {245, 247, 255, 255};
static GXColor ui_gold = {255, 204, 0, 255};
static GXColor ui_muted = {178, 190, 220, 255};
static GXColor ui_dark = {7, 12, 30, 242};
static GXColor ui_black = {5, 8, 20, 250};
static GXColor ui_red = {204, 31, 24, 255};
static GXColor ui_panel_color = {12, 18, 40, 238};
static GXColor ui_panel_soft = {15, 22, 48, 224};
static GXColor ui_border = {126, 145, 205, 255};
static GXColor ui_border_dim = {67, 78, 112, 255};
static GXColor ui_blue = {83, 119, 242, 255};
static GXColor ui_purple = {188, 73, 255, 255};

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

static void draw_panel_frame(float x, float y, float width, float height,
                             GXColor border, GXColor bg)
{
    ui_panel(x, y, width, height, border, ui_white);
    ui_panel(x + .16f, y + .16f,
             width - .32f, height - .32f,
             bg, ui_white);
}

static void draw_node(float x, const char* label,
                      bool active, bool complete)
{
    GXColor bg = active ? ui_gold : complete ? ui_red : ui_panel_soft;
    GXColor fg = active ? ui_black : ui_white;
    HSD_Text* text = ui_panel(x, -13.15f, 4.20f, 1.38f, bg, fg);
    char encoded[128];
    int entry;

    if (text == NULL)
        return;

    ui_encode(encoded, label);
    entry = HSD_SisLib_803A6B98(text, 13.0f, 10.0f, "%s", encoded);
    HSD_SisLib_803A7548(text, entry, 1.42f, 1.42f);
}

static const char* reward_icon_letter(const RogueReward* reward)
{
    const char* category = reward_category(reward);

    if (!strcmp(category, "SPECIAL"))
        return "S";
    if (!strcmp(category, "DEFENSE"))
        return "D";
    if (!strcmp(category, "MOVEMENT"))
        return "M";
    if (!strcmp(category, "PASSIVE"))
        return "P";
    return "B";
}

static void draw_reward_card(float x, int index, bool selected)
{
    RogueReward* reward = &g_rogue_run.current_rewards[index];
    GXColor border = selected ? ui_gold : ui_border;
    GXColor title = selected ? ui_gold : ui_white;
    char detail[180];
    char line1[64];
    char line2[64];

    Rogue_DescribeReward(reward, detail, sizeof(detail));
    split_description(detail,
                      line1, sizeof(line1),
                      line2, sizeof(line2),
                      29);

    draw_panel_frame(x, -8.20f, 9.20f, 4.70f,
                     border, ui_panel_color);

    draw_panel_frame(x + .48f, -7.70f, 1.45f, 1.45f,
                     border, ui_black);
    ui_at(x + .94f, -7.42f, .0175f, title,
          "%s", reward_icon_letter(reward));

    ui_at(x + 2.18f, -7.70f, .0195f, title,
          "%s", reward->name);
    ui_at(x + 2.18f, -6.66f, .0108f, ui_muted,
          "%s / %s",
          reward_category(reward),
          Rogue_RarityName(reward->rarity));

    ui_panel(x + .48f, -5.94f, 8.22f, .08f,
             selected ? ui_gold : ui_border_dim, ui_white);

    ui_at(x + .55f, -5.50f, .0114f, ui_white,
          "%s", line1);
    if (line2[0]) {
        ui_at(x + .55f, -4.70f, .0114f, ui_white,
              "%s", line2);
    }
}

static void draw_fight_plate(float x, int side,
                             const RogueEncounter* encounter,
                             bool selected, bool locked)
{
    GXColor accent = selected ? ui_gold :
                     side == 0 ? ui_blue : ui_purple;
    GXColor title = selected ? ui_gold : ui_white;
    char opponent[96];
    const char* stage;
    const char* type;
    char detail[120];

    encounter_name(opponent, sizeof(opponent), encounter);
    stage = RogueRoute_StageName(encounter->stage);
    type = RogueRoute_TypeName(encounter->type);

    if (locked) {
        snprintf(detail, sizeof(detail), "UPGRADE FIRST");
    } else if (encounter->modifier && *encounter->modifier) {
        snprintf(detail, sizeof(detail), "%s", encounter->modifier);
    } else if (encounter->enemy_count > 0) {
        snprintf(detail, sizeof(detail), "%d STOCK%s",
                 encounter->enemies[0].stocks,
                 encounter->enemies[0].stocks == 1 ? "" : "S");
    } else {
        snprintf(detail, sizeof(detail), "STANDARD");
    }

    draw_panel_frame(x, 4.25f, 14.35f, 3.45f,
                     accent, ui_panel_color);

    ui_at(x + .55f, 4.62f, .0198f, title,
          "%s", opponent);

    ui_panel(x + .55f, 5.76f, 13.20f, .07f,
             ui_border_dim, ui_white);

    ui_at(x + .55f, 6.03f, .0113f, ui_white,
          "%s", stage);
    ui_at(x + 8.95f, 6.03f, .0113f, accent,
          "%s", type);

    ui_at(x + .55f, 6.88f, .0098f, ui_muted,
          "%s", detail);
}

static void draw_center_vs(void)
{
    ui_panel(-1.30f, -.35f, 2.60f, 1.55f,
             ui_black, ui_white);
    ui_at(-.72f, -.05f, .0240f, ui_red, "VS");
}

static void draw_bottom_bar(void)
{
    const RogueStats* stats = &g_rogue_run.stats;
    int score =
        g_rogue_run.wins * 10000 +
        g_rogue_run.currency * 100;
    char abilities[128];
    char summary[128];

    draw_panel_frame(-19.55f, 8.35f, 39.10f, 6.10f,
                     ui_border, ui_black);

    ui_at(-9.60f, 8.88f, .0208f, ui_white,
          "CURRENT CHARACTER BUILD / UPGRADES");

    ui_panel(-17.70f, 10.15f, 35.40f, .08f,
             ui_border_dim, ui_white);

    snprintf(abilities, sizeof(abilities),
             "N %.10s  /  S %.10s  /  U %.10s  /  D %.10s",
             ability_name(ROGUE_ABILITY_NEUTRAL),
             ability_name(ROGUE_ABILITY_SIDE),
             ability_name(ROGUE_ABILITY_UP),
             ability_name(ROGUE_ABILITY_DOWN));
    ui_at(-14.95f, 10.55f, .0122f, ui_white,
          "%s", abilities);

    if (has_reward) {
        snprintf(summary, sizeof(summary),
                 "GOLD +%d  TOTAL %d  SCORE %d   DMG %.0f%%  DEF %.0f%%",
                 gold_gain,
                 g_rogue_run.currency,
                 score,
                 stats->damage_dealt * 100.0f,
                 stats->damage_received * 100.0f);
    } else {
        snprintf(summary, sizeof(summary),
                 "GOLD %d  SCORE %d   DMG %.0f%%  DEF %.0f%%",
                 g_rogue_run.currency,
                 score,
                 stats->damage_dealt * 100.0f,
                 stats->damage_received * 100.0f);
    }

    ui_at(-12.70f, 11.72f, .0108f, ui_muted,
          "%s", summary);
}

static void draw_controls(void)
{
    if (has_reward && !upgrade_chosen) {
        ui_at(-8.35f, 13.35f, .0115f, ui_white,
              "LEFT / RIGHT: UPGRADE     A: SELECT     B: BUILD");
    } else if (target_act_floor >= ROGUE_FLOORS_PER_ACT) {
        ui_at(-7.05f, 13.35f, .0115f, ui_white,
              "UPGRADE SELECTED     CONTINUING TO SHOP / REST");
    } else if (confirm_timer > 0) {
        ui_at(-5.65f, 13.35f, .0115f, ui_gold,
              "MATCH SET     LOADING VS SCREEN");
    } else {
        ui_at(-8.00f, 13.35f, .0115f, ui_white,
              "LEFT / RIGHT: FIGHT       A: SELECT     B: BUILD");
    }
}

static void draw_build(void)
{
    const RogueStats* stats = &g_rogue_run.stats;

    ui_clear();

    draw_panel_frame(-17.75f, -12.15f, 35.50f, 23.35f,
                     ui_border, ui_black);

    ui_at(-9.25f, -10.75f, .0255f, ui_gold,
          "CURRENT CHARACTER BUILD");
    ui_panel(-15.50f, -9.25f, 31.00f, .10f,
             ui_border_dim, ui_white);

    ui_at(-14.75f, -7.65f, .0175f, ui_white,
          "NEUTRAL     %s",
          ability_name(ROGUE_ABILITY_NEUTRAL));
    ui_at(-14.75f, -5.30f, .0175f, ui_white,
          "SIDE        %s",
          ability_name(ROGUE_ABILITY_SIDE));
    ui_at(-14.75f, -2.95f, .0175f, ui_white,
          "UP          %s",
          ability_name(ROGUE_ABILITY_UP));
    ui_at(-14.75f, -.60f, .0175f, ui_white,
          "DOWN        %s",
          ability_name(ROGUE_ABILITY_DOWN));

    ui_panel(-15.50f, 1.15f, 31.00f, .10f,
             ui_border_dim, ui_white);

    ui_at(-14.75f, 2.30f, .0155f, ui_white,
          "DAMAGE %.0f%%      DEFENSE %.0f%%",
          stats->damage_dealt * 100.0f,
          stats->damage_received * 100.0f);
    ui_at(-14.75f, 4.35f, .0155f, ui_white,
          "RUN %.0f%%         SHIELD %.0f%%",
          stats->run_speed * 100.0f,
          stats->shield_health * 100.0f);
    ui_at(-14.75f, 6.40f, .0155f, ui_white,
          "AIR +%.0f%%        JUMP +%.0f%%        EXTRA +%d",
          stats->air_control_bonus * 100.0f,
          stats->jump_height_bonus * 100.0f,
          stats->extra_jumps);
    ui_at(-14.75f, 8.45f, .0155f, ui_white,
          "KNOCKBACK +%.0f%%  RESIST %.0f%%",
          stats->knockback_dealt_bonus * 100.0f,
          stats->knockback_resistance * 100.0f);

    ui_at(-2.65f, 10.05f, .0125f, ui_gold,
          "B: RETURN");
}

/*
 * TARGET-RENDER POLISH PASS
 *
 * Preserve the stable native GS_INTRO_EASY host and two-step progression
 * state machine. Only presentation changes here.
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

    ui_panel(-20.0f, -15.0f, 40.0f, 5.55f,
             ui_black, ui_white);

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

    ui_at(-4.60f, -10.60f, .0175f, ui_white,
          "ACT %d   -   FLOOR %d",
          route->act, target_floor);

    ui_panel(-20.0f, -9.45f, 40.0f, 1.25f,
             ui_black, ui_white);

    if (has_reward && !upgrade_chosen) {
        ui_at(-4.35f, -9.20f, .0168f, ui_gold,
              "CHOOSE UPGRADE");

        ui_panel(-20.0f, -8.20f, 40.0f, 4.95f,
                 ui_dark, ui_white);

        for (i = 0; i < 3; ++i) {
            draw_reward_card(
                -14.75f + i * 9.78f,
                i,
                upgrade_cursor == i);
        }
    } else {
        ui_at(-4.85f, -9.20f, .0168f, ui_gold,
              has_reward ? "CHOOSE NEXT FIGHT" : "CHOOSE FIRST FIGHT");
    }

    draw_center_vs();

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
                i == 0 ? -14.75f : .40f,
                i,
                &round->choices[i],
                selected,
                !upgrade_chosen);
        }
    } else {
        const char* boss =
            route->boss_generated ?
            RogueRoute_CharacterName(route->boss.enemy_kind) :
            "BOSS";

        draw_panel_frame(-14.75f, 4.25f, 29.50f, 3.45f,
                         upgrade_chosen ? ui_gold : ui_border,
                         ui_panel_color);

        ui_at(-11.70f, 4.72f, .0180f,
              upgrade_chosen ? ui_gold : ui_white,
              "SHOP / REST  ->  %s", boss);

        ui_at(-8.25f, 6.25f, .0105f, ui_muted,
              upgrade_chosen ?
              "UPGRADE LOCKED IN - CONTINUING" :
              "SELECT AN UPGRADE FIRST");
    }

    draw_bottom_bar();
    draw_controls();
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
