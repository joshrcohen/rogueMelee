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

/*
 * PROGRESSION V8: ROGUE ROUTE + CLEAN LAYOUT
 *
 * Stability rules:
 *   - SIS-only rendering; no raw GX callbacks
 *   - split text buffers from V6 remain intact
 *   - Rogue progression retains the 0xC000 SIS arena
 *
 * Presentation:
 *   - the unused retail IrRdMap is hidden for Rogue state 7
 *   - Rogue draws its own six-node route in native 640x480 SIS space
 *   - there is no Rogue background panel behind the route
 *   - reward/fight/build text is constrained to its panel bounds
 */

static HSD_Text* lines[24];
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
static GXColor ui_dark = {7, 12, 30, 244};
static GXColor ui_black = {3, 6, 16, 252};
static GXColor ui_red = {204, 31, 24, 255};
static GXColor ui_panel = {12, 18, 40, 230};
static GXColor ui_panel_soft = {15, 22, 48, 224};
static GXColor ui_blue = {83, 119, 242, 255};
static GXColor ui_purple = {188, 73, 255, 255};
static GXColor ui_glass = {8, 13, 34, 214};
static GXColor ui_shadow = {0, 0, 0, 255};

static void ui_clear(void)
{
    unsigned i;

    for (i = 0; i < line_count; ++i) {
        if (lines[i] != NULL)
            HSD_SisLib_803A5CC4(lines[i]);
    }

    line_count = 0;
}

static HSD_Text* ui_track(HSD_Text* text)
{
    if (text == NULL)
        return NULL;

    if (line_count >= sizeof(lines) / sizeof(lines[0])) {
        HSD_SisLib_803A5CC4(text);
        return NULL;
    }

    lines[line_count++] = text;
    return text;
}

static HSD_Text* ui_rect(float x, float y, float w, float h, GXColor color)
{
    HSD_Text* text = ui_track(HSD_SisLib_803A6754(0, 0));

    if (text == NULL)
        return NULL;

    text->pos_x = x;
    text->pos_y = y;
    text->pos_z = 0.0f;

    /*
     * A 1.0 font scale makes box_size map directly to the native 640x480
     * SIS coordinate system.
     */
    text->font_size.x = 1.0f;
    text->font_size.y = 1.0f;
    text->box_size_x = w;
    text->box_size_y = h;

    text->bg_color = color;
    text->text_color.a = 0;

    return text;
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

static HSD_Text* ui_text_group(void)
{
    HSD_Text* text = ui_track(HSD_SisLib_803A6754(0, 0));

    if (text == NULL)
        return NULL;

    text->pos_x = 0.0f;
    text->pos_y = 0.0f;
    text->pos_z = 0.0f;
    text->font_size.x = 1.0f;
    text->font_size.y = 1.0f;
    text->default_kerning = 1;
    text->text_color = ui_white;

    return text;
}

static int ui_entry_raw(HSD_Text* text, float x, float y, float scale,
                        GXColor* color, const char* value)
{
    char encoded[128];
    int entry;

    if (text == NULL || value == NULL)
        return -1;

    ui_encode(encoded, value);
    entry = HSD_SisLib_803A6B98(text, x, y, "%s", encoded);
    HSD_SisLib_803A7548(text, entry, scale, scale);

    if (color != NULL)
        HSD_SisLib_803A74F0(text, entry, color);

    return entry;
}

static int ui_entryf(HSD_Text* text, float x, float y, float scale,
                     GXColor* color, const char* fmt, ...)
{
    char raw[256];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(raw, sizeof(raw), fmt, ap);
    va_end(ap);

    return ui_entry_raw(text, x, y, scale, color, raw);
}

static void ui_title(HSD_Text* text, float x, float y, float scale,
                     GXColor* color, const char* value)
{
    ui_entry_raw(text, x + 1.0f, y + 1.0f, scale, &ui_shadow, value);
    ui_entry_raw(text, x, y, scale, color, value);
}

static const char* copy_wrapped_line(const char* text,
                                     char* out, unsigned out_size,
                                     int width)
{
    int len;
    int cut;

    if (out_size == 0)
        return text;

    out[0] = 0;

    if (text == NULL)
        return "";

    while (*text == ' ')
        ++text;

    if (*text == 0)
        return text;

    len = strlen(text);
    cut = len > width ? width : len;

    if (len > width) {
        while (cut > 0 && text[cut] != ' ')
            --cut;

        if (cut <= 0)
            cut = width;
    }

    if ((unsigned) cut >= out_size)
        cut = out_size - 1;

    memcpy(out, text, cut);
    out[cut] = 0;

    text += cut;
    while (*text == ' ')
        ++text;

    return text;
}

static void wrap_description3(const char* text,
                              char* line1, unsigned line1_size,
                              char* line2, unsigned line2_size,
                              char* line3, unsigned line3_size,
                              int width)
{
    const char* next;

    next = copy_wrapped_line(text, line1, line1_size, width);
    next = copy_wrapped_line(next, line2, line2_size, width);
    copy_wrapped_line(next, line3, line3_size, width);
}

static float fit_text_scale(const char* value, float base,
                            float minimum, int max_chars)
{
    int len;
    float scale;

    if (value == NULL)
        return base;

    len = strlen(value);
    if (len <= max_chars || len <= 0)
        return base;

    scale = base * ((float) max_chars / (float) len);
    return scale < minimum ? minimum : scale;
}

static void trim_suffix(char* text, const char* suffix)
{
    size_t text_len;
    size_t suffix_len;

    if (text == NULL || suffix == NULL)
        return;

    text_len = strlen(text);
    suffix_len = strlen(suffix);

    if (suffix_len <= text_len &&
        strcmp(text + text_len - suffix_len, suffix) == 0)
    {
        text[text_len - suffix_len] = 0;
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

    if (id == ROGUE_ABILITY_NATIVE)
        id = Rogue_AbilityForOpponent(g_rogue_run.player_kind, slot);

    def = Rogue_GetAbility(id);
    return def ? def->name : "Native";
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

/* ------------------------------------------------------------------------- */
/* Safe SIS background panels                                                */
/* ------------------------------------------------------------------------- */

static void draw_base_panels(void)
{
    bool left_selected;
    bool right_selected;

    /*
     * Intentionally draw NOTHING behind the route. The top of IntroEasy is
     * already black; V8's six-node route sits directly on it.
     */

    /* Mask only the retail STAGE title band. */
    ui_rect(0.0f, 74.0f, 640.0f, 49.0f, ui_black);

    /*
     * Mask the small native ALLY tag without covering the red VS graphic.
     * This is deliberately narrow instead of another full-width overlay.
     */
    ui_rect(250.0f, 229.0f, 58.0f, 24.0f, ui_glass);

    left_selected =
        upgrade_chosen &&
        (fight_locked >= 0 ? fight_locked == 0 : fight_cursor == 0);
    right_selected =
        upgrade_chosen &&
        (fight_locked >= 0 ? fight_locked == 1 : fight_cursor == 1);

    if (target_act_floor < ROGUE_FLOORS_PER_ACT) {
        ui_rect(22.0f, 286.0f, 282.0f, 50.0f, ui_glass);
        ui_rect(336.0f, 286.0f, 282.0f, 50.0f, ui_glass);

        ui_rect(22.0f, 286.0f, 282.0f, 3.0f,
                left_selected ? ui_gold : ui_blue);
        ui_rect(336.0f, 286.0f, 282.0f, 3.0f,
                right_selected ? ui_gold : ui_purple);
    } else {
        ui_rect(78.0f, 286.0f, 484.0f, 50.0f, ui_glass);
    }

    ui_rect(0.0f, 350.0f, 640.0f, 130.0f, ui_black);
}

static void draw_reward_card_panels(void)
{
    static const float x[3] = {24.0f, 222.0f, 420.0f};
    int i;

    ui_rect(x[upgrade_cursor] - 3.0f, 118.0f,
            202.0f, 111.0f, ui_gold);

    for (i = 0; i < 3; ++i) {
        ui_rect(x[i], 121.0f, 196.0f, 105.0f,
                i == upgrade_cursor ? ui_dark : ui_panel);
    }
}

static void draw_full_build_panels(void)
{
    ui_rect(0.0f, 0.0f, 640.0f, 480.0f, ui_black);
    ui_rect(62.0f, 49.0f, 516.0f, 354.0f, ui_dark);

    ui_rect(88.0f, 115.0f, 214.0f, 58.0f, ui_panel_soft);
    ui_rect(338.0f, 115.0f, 214.0f, 58.0f, ui_panel_soft);
    ui_rect(88.0f, 191.0f, 214.0f, 58.0f, ui_panel_soft);
    ui_rect(338.0f, 191.0f, 214.0f, 58.0f, ui_panel_soft);
    ui_rect(88.0f, 271.0f, 464.0f, 78.0f, ui_panel_soft);
}

/* ------------------------------------------------------------------------- */
/* Text layer                                                                */
/* ------------------------------------------------------------------------- */

static void draw_route_map(HSD_Text* text)
{
    static const char* names[6] = {
        "CLEAR", "NEXT", "ELITE", "MATCH 4", "SHOP", "BOSS"
    };
    static const float node_x[6] = {
        73.0f, 171.0f, 269.0f, 367.0f, 465.0f, 563.0f
    };
    static const float label_x[6] = {
        55.0f, 154.0f, 251.0f, 339.0f, 449.0f, 548.0f
    };
    const RogueRoute* route = &g_rogue_run.route;
    int active;
    int i;

    active = target_act_floor - 1;
    if (active < 0)
        active = 0;
    if (active > 4)
        active = 4;

    /*
     * Native-looking road line. All geometry lives inside this one HSD_Text
     * command buffer, avoiding the object-count problems from older passes.
     */
    for (i = 0; i < 5; ++i) {
        ui_entry_raw(text, node_x[i] + 14.0f, 18.0f,
                     .23f, &ui_muted, "------------");
    }

    for (i = 0; i < 6; ++i) {
        GXColor* node_color;
        GXColor* label_color;

        if (i < active) {
            node_color = &ui_red;
            label_color = &ui_white;
        } else if (i == active) {
            node_color = &ui_gold;
            label_color = &ui_gold;
        } else {
            node_color = &ui_muted;
            label_color = &ui_muted;
        }

        if (i == active) {
            ui_entry_raw(text, node_x[i] + 3.0f, 1.0f,
                         .24f, &ui_gold, "V");
        }

        ui_entry_raw(text, node_x[i], 11.0f,
                     i == 3 ? .34f : .40f,
                     node_color,
                     i == 3 ? "[]" : "O");

        ui_entry_raw(text, label_x[i], 36.0f,
                     .25f, label_color, names[i]);
    }

    ui_title(text, 253.0f, 58.0f, .32f, &ui_white, "ACT");
    ui_entryf(text, 281.0f, 58.0f, .32f, &ui_white, "%d", route->act);
    ui_entry_raw(text, 304.0f, 58.0f, .32f, &ui_muted, "-");
    ui_title(text, 326.0f, 58.0f, .32f, &ui_white, "FLOOR");
    ui_entryf(text, 371.0f, 58.0f, .32f, &ui_white, "%d", target_floor);
}

static void draw_phase_text(HSD_Text* text)
{
    const char* phase;
    float x;

    if (has_reward && !upgrade_chosen) {
        phase = "CHOOSE UPGRADE";
        x = 248.0f;
    } else if (has_reward) {
        phase = "CHOOSE NEXT FIGHT";
        x = 225.0f;
    } else {
        phase = "CHOOSE FIRST FIGHT";
        x = 223.0f;
    }

    ui_title(text, x, 88.0f, .40f, &ui_gold, phase);

    if (has_reward && upgrade_chosen && upgrade_taken >= 0) {
        ui_entryf(text, 247.0f, 108.0f, .24f, &ui_muted,
                  "LOCKED: %s",
                  g_rogue_run.current_rewards[upgrade_taken].name);
    }
}

static void draw_reward_card_text(HSD_Text* text, int i)
{
    static const float x[3] = {34.0f, 232.0f, 430.0f};
    RogueReward* reward;
    GXColor* accent;
    char meta[80];
    char detail[180];
    char line1[64];
    char line2[64];
    char line3[64];
    float title_scale;

    if (text == NULL || i < 0 || i >= 3)
        return;

    reward = &g_rogue_run.current_rewards[i];
    accent = upgrade_cursor == i ? &ui_gold : &ui_white;

    Rogue_DescribeReward(reward, detail, sizeof(detail));

    /*
     * The full build screen already explains slot retention. Removing this
     * sentence from ability cards keeps the choice card concise.
     */
    trim_suffix(detail, " Other slots stay equipped.");

    wrap_description3(detail,
                      line1, sizeof(line1),
                      line2, sizeof(line2),
                      line3, sizeof(line3),
                      22);

    snprintf(meta, sizeof(meta), "%s / %s",
             reward_category(reward),
             Rogue_RarityName(reward->rarity));

    title_scale = fit_text_scale(reward->name, .39f, .28f, 16);

    ui_title(text, x[i], 135.0f, .34f,
             accent, reward_icon_letter(reward));

    ui_title(text, x[i] + 30.0f, 132.0f,
             title_scale, accent, reward->name);

    ui_entry_raw(text, x[i] + 30.0f, 151.0f,
                 .23f, &ui_muted, meta);

    ui_entry_raw(text, x[i], 175.0f,
                 .24f, &ui_white, line1);

    if (line2[0]) {
        ui_entry_raw(text, x[i], 193.0f,
                     .24f, &ui_white, line2);
    }

    if (line3[0]) {
        ui_entry_raw(text, x[i], 211.0f,
                     .24f, &ui_white, line3);
    }
}

static void draw_fight_text(HSD_Text* text)
{
    const RogueRouteRound* round = RogueRoute_Current(&g_rogue_run.route);
    int i;

    if (target_act_floor >= ROGUE_FLOORS_PER_ACT ||
        round == NULL || !round->generated)
    {
        const char* boss =
            g_rogue_run.route.boss_generated ?
            RogueRoute_CharacterName(g_rogue_run.route.boss.enemy_kind) :
            "BOSS";

        ui_title(text, 116.0f, 296.0f, .41f,
                 upgrade_chosen ? &ui_gold : &ui_white,
                 "SHOP / REST AREA");

        ui_entryf(text, 322.0f, 297.0f, .31f,
                  &ui_muted, "BOSS: %s", boss);

        if (upgrade_chosen) {
            ui_entry_raw(text, 226.0f, 320.0f, .23f,
                         &ui_muted,
                         "UPGRADE LOCKED IN - CONTINUING");
        }
        return;
    }

    for (i = 0; i < 2; ++i) {
        const RogueEncounter* encounter = &round->choices[i];
        float x = i == 0 ? 34.0f : 348.0f;
        GXColor* side = i == 0 ? &ui_blue : &ui_purple;
        bool selected =
            upgrade_chosen &&
            (fight_locked >= 0 ?
                 fight_locked == i :
                 fight_cursor == i);
        GXColor* title_color =
            selected ? &ui_gold : side;
        char name[96];
        char meta[120];
        char detail[120];
        float name_scale;
        float meta_scale;

        encounter_name(name, sizeof(name), encounter);

        snprintf(meta, sizeof(meta), "%s / %s",
                 RogueRoute_StageName(encounter->stage),
                 RogueRoute_TypeName(encounter->type));

        if (encounter->modifier && *encounter->modifier) {
            snprintf(detail, sizeof(detail), "%s", encounter->modifier);
        } else if (encounter->enemy_count > 0) {
            snprintf(detail, sizeof(detail), "%d STOCK%s",
                     encounter->enemies[0].stocks,
                     encounter->enemies[0].stocks == 1 ? "" : "S");
        } else {
            snprintf(detail, sizeof(detail), "STANDARD");
        }

        name_scale = fit_text_scale(name, .43f, .30f, 19);
        meta_scale = fit_text_scale(meta, .26f, .20f, 31);

        ui_title(text, x, 296.0f, name_scale, title_color, name);
        ui_entry_raw(text, x, 315.0f, meta_scale, side, meta);

        if (upgrade_chosen) {
            ui_entry_raw(text, x, 329.0f,
                         .22f, &ui_muted, detail);
        }
    }
}

static void draw_build_strip_text(HSD_Text* text)
{
    static const float x[4] = {
        31.0f, 183.0f, 335.0f, 487.0f
    };
    static const char* keys[4] = {"N", "S", "U", "D"};
    const char* controls;
    int score =
        g_rogue_run.wins * 10000 +
        g_rogue_run.currency * 100;
    int i;

    ui_title(text, 197.0f, 361.0f, .37f,
             &ui_white,
             "CURRENT CHARACTER BUILD / UPGRADES");

    for (i = 0; i < 4; ++i) {
        const char* name = ability_name(i);
        float scale = fit_text_scale(name, .28f, .22f, 15);

        ui_entry_raw(text, x[i], 393.0f,
                     .31f, &ui_gold, keys[i]);

        ui_entry_raw(text, x[i] + 23.0f, 393.0f,
                     scale, &ui_white, name);
    }

    if (has_reward) {
        ui_entryf(text, 206.0f, 424.0f, .24f, &ui_muted,
                  "GOLD +%d   TOTAL %d   SCORE %d",
                  gold_gain,
                  g_rogue_run.currency,
                  score);
    } else {
        ui_entryf(text, 229.0f, 424.0f, .24f, &ui_muted,
                  "GOLD %d   SCORE %d",
                  g_rogue_run.currency,
                  score);
    }

    if (has_reward && !upgrade_chosen) {
        controls =
            "LEFT / RIGHT: UPGRADE     A: SELECT     B: BUILD";
    } else if (target_act_floor >= ROGUE_FLOORS_PER_ACT) {
        controls =
            "UPGRADE SELECTED     CONTINUING TO SHOP / REST";
    } else if (confirm_timer > 0) {
        controls =
            "MATCH SET     LOADING VS SCREEN";
    } else {
        controls =
            "LEFT / RIGHT: FIGHT     A: SELECT     B: BUILD";
    }

    ui_entry_raw(text, 190.0f, 452.0f, .27f,
                 confirm_timer > 0 ? &ui_gold : &ui_white,
                 controls);
}

static void draw_full_build_text(HSD_Text* text)
{
    const RogueStats* stats = &g_rogue_run.stats;

    ui_title(text, 188.0f, 69.0f, .62f,
             &ui_gold, "CURRENT CHARACTER BUILD");

    ui_entry_raw(text, 108.0f, 129.0f, .32f,
                 &ui_muted, "NEUTRAL");
    ui_title(text, 108.0f, 148.0f, .47f,
             &ui_white, ability_name(ROGUE_ABILITY_NEUTRAL));

    ui_entry_raw(text, 358.0f, 129.0f, .32f,
                 &ui_muted, "SIDE");
    ui_title(text, 358.0f, 148.0f, .47f,
             &ui_white, ability_name(ROGUE_ABILITY_SIDE));

    ui_entry_raw(text, 108.0f, 205.0f, .32f,
                 &ui_muted, "UP");
    ui_title(text, 108.0f, 224.0f, .47f,
             &ui_white, ability_name(ROGUE_ABILITY_UP));

    ui_entry_raw(text, 358.0f, 205.0f, .32f,
                 &ui_muted, "DOWN");
    ui_title(text, 358.0f, 224.0f, .47f,
             &ui_white, ability_name(ROGUE_ABILITY_DOWN));

    ui_entryf(text, 108.0f, 289.0f, .39f, &ui_white,
              "DAMAGE %.0f%%       DEFENSE %.0f%%",
              stats->damage_dealt * 100.0f,
              stats->damage_received * 100.0f);

    ui_entryf(text, 108.0f, 309.0f, .39f, &ui_white,
              "RUN %.0f%%          SHIELD %.0f%%",
              stats->run_speed * 100.0f,
              stats->shield_health * 100.0f);

    ui_entryf(text, 108.0f, 329.0f, .39f, &ui_white,
              "AIR +%.0f%%   JUMP +%.0f%%   EXTRA +%d",
              stats->air_control_bonus * 100.0f,
              stats->jump_height_bonus * 100.0f,
              stats->extra_jumps);

    ui_entryf(text, 108.0f, 349.0f, .39f, &ui_white,
              "KNOCKBACK +%.0f%%   RESIST %.0f%%",
              stats->knockback_dealt_bonus * 100.0f,
              stats->knockback_resistance * 100.0f);

    ui_entry_raw(text, 278.0f, 379.0f, .36f,
                 &ui_gold, "B: RETURN");
}

static void draw_progression(void)
{
    HSD_Text* text;
    int i;

    ui_clear();

    if (build_open) {
        draw_full_build_panels();

        text = ui_text_group();
        if (text != NULL)
            draw_full_build_text(text);

        return;
    }

    /*
     * Background-only SIS objects are created first so all text renders above
     * them in the same retail IntroEasy SIS context.
     */
    draw_base_panels();

    if (has_reward && !upgrade_chosen)
        draw_reward_card_panels();

    /*
     * Do NOT put the whole progression screen into one dynamic SIS command
     * buffer. HSD_SisLib_803A6B98 grows that buffer by reallocating it in the
     * scene's SIS arena. The post-Stage-Clear reward screen adds substantially
     * more entries than the initial chooser, which is exactly the path that
     * was hanging.
     *
     * Keep every section in a modest independent buffer instead.
     */
    text = ui_text_group();
    if (text == NULL)
        return;
    draw_route_map(text);
    draw_phase_text(text);

    if (has_reward && !upgrade_chosen) {
        for (i = 0; i < 3; ++i) {
            text = ui_text_group();
            if (text == NULL)
                return;
            draw_reward_card_text(text, i);
        }
    }

    text = ui_text_group();
    if (text == NULL)
        return;
    draw_fight_text(text);

    text = ui_text_group();
    if (text == NULL)
        return;
    draw_build_strip_text(text);
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
