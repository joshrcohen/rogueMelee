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
#include <sysdolphin/baselib/hsd_3915.h>
#include <sysdolphin/baselib/sislib.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

RogueProgressionIntroData g_rogue_progression_intro;

/*
 * PROGRESSION V4: NATIVE PANEL LAYER
 *
 * Architecture:
 *   - one HSD_Text object is used only as a render-callback host
 *   - that callback draws every custom panel/border with DrawRectangle()
 *   - one normal HSD_Text object owns all Rogue text entries
 *
 * HSD_SisLib_803A84BC sets up the native SIS camera, projection and alpha
 * blending before invoking HSD_Text::render_callback. That gives us a safe
 * 640x480 UI drawing surface without creating another camera or another SIS
 * context.
 *
 * The panel-driver object is created before the Rogue text object. Objects on
 * the same SIS GX link are appended in creation order, so native IntroEasy UI
 * renders first, then our panels mask unwanted native clutter, then our text
 * renders on top.
 *
 * This replaces the old "HSD_Text with a giant background box" panel hack.
 */

static HSD_Text* lines[8];
static unsigned line_count;
static HSD_Text* panel_driver;

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
static GXColor ui_panel_color = {12, 18, 40, 232};
static GXColor ui_panel_soft = {15, 22, 48, 224};
static GXColor ui_border = {126, 145, 205, 255};
static GXColor ui_border_dim = {67, 78, 112, 255};
static GXColor ui_blue = {83, 119, 242, 255};
static GXColor ui_purple = {188, 73, 255, 255};
static GXColor ui_glass = {8, 13, 34, 214};
static GXColor ui_glass_soft = {8, 13, 34, 172};
static GXColor ui_shadow = {0, 0, 0, 255};
static GXColor ui_node_future = {36, 43, 64, 255};

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

static HSD_Text* ui_text_group(void)
{
    HSD_Text* text;

    if (line_count >= sizeof(lines) / sizeof(lines[0]))
        return NULL;

    text = HSD_SisLib_803A6754(0, 0);
    if (text == NULL)
        return NULL;

    lines[line_count++] = text;

    /*
     * Native IntroEasy uses a 640x480 SIS canvas. Keep the text object at
     * screen origin and use per-entry pixel coordinates / scale.
     */
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
    /*
     * A one-pixel black duplicate substantially improves readability against
     * bright fighter renders without needing a new HSD_Text object.
     */
    ui_entry_raw(text, x + 1.0f, y + 1.0f, scale, &ui_shadow, value);
    ui_entry_raw(text, x, y, scale, color, value);
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
/* Native panel layer                                                        */
/* ------------------------------------------------------------------------- */

static void panel_fill(float x, float y, float w, float h, GXColor* color)
{
    DrawRectangle(x, y, w, h, color);
}

static void panel_box(float x, float y, float w, float h,
                      float border, GXColor* border_color,
                      GXColor* fill_color)
{
    panel_fill(x, y, w, h, border_color);

    if (w > border * 2.0f && h > border * 2.0f) {
        panel_fill(x + border, y + border,
                   w - border * 2.0f, h - border * 2.0f,
                   fill_color);
    }
}

static void panel_outline(float x, float y, float w, float h,
                          float thickness, GXColor* color)
{
    panel_fill(x, y, w, thickness, color);
    panel_fill(x, y + h - thickness, w, thickness, color);
    panel_fill(x, y, thickness, h, color);
    panel_fill(x + w - thickness, y, thickness, h, color);
}

static void draw_route_panels(void)
{
    static const float node_x[6] = {
        76.0f, 174.0f, 272.0f, 370.0f, 468.0f, 566.0f
    };
    const RogueRoute* route = &g_rogue_run.route;
    int i;

    panel_fill(0.0f, 0.0f, 640.0f, 84.0f, &ui_black);

    panel_fill(76.0f, 25.0f, 490.0f, 2.0f, &ui_border_dim);

    for (i = 0; i < 6; ++i) {
        bool complete = false;
        bool active = false;
        GXColor* fill = &ui_node_future;
        GXColor* border = &ui_border_dim;

        if (i < ROGUE_ROUTE_ROUNDS)
            complete = route->rounds[i].selected >= 0;

        if (target_act_floor < ROGUE_FLOORS_PER_ACT)
            active = i == target_act_floor - 1;
        else
            active = i == 4;

        if (active) {
            fill = &ui_gold;
            border = &ui_gold;
            panel_fill(node_x[i] - 10.0f, 17.0f,
                       20.0f, 20.0f, &ui_glass_soft);
        } else if (complete) {
            fill = &ui_red;
            border = &ui_red;
        }

        panel_box(node_x[i] - 7.0f, 20.0f,
                  14.0f, 14.0f, 2.0f, border, fill);
    }

    panel_fill(0.0f, 82.0f, 640.0f, 2.0f, &ui_border_dim);
}

static void draw_phase_panel(void)
{
    /*
     * This opaque strip intentionally covers the retail giant STAGE title.
     * It ends before the fighter presentation becomes the visual focus.
     */
    panel_fill(0.0f, 84.0f, 640.0f, 37.0f, &ui_black);
    panel_fill(0.0f, 119.0f, 640.0f, 2.0f, &ui_border_dim);
}

static void draw_reward_panels(void)
{
    static const float card_x[3] = { 27.0f, 225.0f, 423.0f };
    int i;

    /*
     * A light dimmer removes the remaining retail STAGE lettering from the
     * card band while preserving fighter art below the cards.
     */
    panel_fill(0.0f, 121.0f, 640.0f, 104.0f, &ui_glass_soft);

    for (i = 0; i < 3; ++i) {
        bool selected = upgrade_cursor == i;
        GXColor* border = selected ? &ui_gold : &ui_border;
        GXColor* fill = selected ? &ui_dark : &ui_glass;

        if (selected) {
            GXColor glow = {255, 204, 0, 86};
            panel_outline(card_x[i] - 3.0f, 126.0f,
                          190.0f, 96.0f, 3.0f, &glow);
        }

        panel_box(card_x[i], 129.0f, 184.0f, 90.0f,
                  selected ? 3.0f : 2.0f, border, fill);

        panel_fill(card_x[i] + 2.0f, 131.0f,
                   4.0f, 86.0f, border);

        panel_box(card_x[i] + 12.0f, 140.0f,
                  27.0f, 27.0f, 2.0f,
                  border, &ui_panel_soft);

        panel_fill(card_x[i] + 12.0f, 177.0f,
                   160.0f, 1.0f, &ui_border_dim);
    }
}

static void draw_fight_panels(void)
{
    const RogueRouteRound* round = RogueRoute_Current(&g_rogue_run.route);
    int i;

    if (target_act_floor >= ROGUE_FLOORS_PER_ACT ||
        round == NULL || !round->generated)
    {
        panel_box(78.0f, 326.0f, 484.0f, 56.0f,
                  2.0f,
                  upgrade_chosen ? &ui_gold : &ui_border,
                  &ui_glass);
        return;
    }

    for (i = 0; i < 2; ++i) {
        float x = i == 0 ? 22.0f : 336.0f;
        GXColor* side = i == 0 ? &ui_blue : &ui_purple;
        bool selected =
            upgrade_chosen &&
            (fight_locked >= 0 ?
                 fight_locked == i :
                 fight_cursor == i);
        GXColor* border = selected ? &ui_gold : side;

        panel_box(x, 326.0f, 282.0f, 56.0f,
                  selected ? 3.0f : 2.0f,
                  border, &ui_glass);

        panel_fill(x + 2.0f, 328.0f, 278.0f, 3.0f, side);

        if (!upgrade_chosen) {
            GXColor lock_tint = {5, 8, 20, 78};
            panel_fill(x + 2.0f, 331.0f, 278.0f, 49.0f, &lock_tint);
        }
    }
}

static void draw_build_strip_panels(void)
{
    static const float chip_x[4] = {
        38.0f, 184.0f, 330.0f, 476.0f
    };
    int i;

    panel_fill(0.0f, 386.0f, 640.0f, 94.0f, &ui_black);
    panel_fill(0.0f, 386.0f, 640.0f, 2.0f, &ui_border_dim);

    for (i = 0; i < 4; ++i) {
        panel_box(chip_x[i], 414.0f, 128.0f, 25.0f,
                  1.0f, &ui_border_dim, &ui_panel_soft);
        panel_box(chip_x[i] + 4.0f, 417.0f, 20.0f, 19.0f,
                  1.0f, &ui_gold, &ui_dark);
    }
}

static void draw_full_build_panels(void)
{
    panel_fill(0.0f, 0.0f, 640.0f, 480.0f, &ui_black);

    panel_box(62.0f, 49.0f, 516.0f, 354.0f,
              2.0f, &ui_border, &ui_dark);
    panel_fill(62.0f, 49.0f, 516.0f, 4.0f, &ui_gold);

    panel_box(88.0f, 115.0f, 214.0f, 58.0f,
              1.0f, &ui_border_dim, &ui_panel_soft);
    panel_box(338.0f, 115.0f, 214.0f, 58.0f,
              1.0f, &ui_border_dim, &ui_panel_soft);
    panel_box(88.0f, 191.0f, 214.0f, 58.0f,
              1.0f, &ui_border_dim, &ui_panel_soft);
    panel_box(338.0f, 191.0f, 214.0f, 58.0f,
              1.0f, &ui_border_dim, &ui_panel_soft);

    panel_box(88.0f, 271.0f, 464.0f, 78.0f,
              1.0f, &ui_border_dim, &ui_panel_soft);
}

static void progression_panel_render(void* unused)
{
    (void) unused;

    if (!ui_open)
        return;

    if (build_open) {
        draw_full_build_panels();
        return;
    }

    draw_route_panels();
    draw_phase_panel();

    if (has_reward && !upgrade_chosen)
        draw_reward_panels();

    draw_fight_panels();
    draw_build_strip_panels();
}

static void panel_driver_open(void)
{
    if (panel_driver != NULL)
        return;

    panel_driver = HSD_SisLib_803A6754(0, 0);
    if (panel_driver == NULL)
        return;

    panel_driver->pos_x = 0.0f;
    panel_driver->pos_y = 0.0f;
    panel_driver->pos_z = 0.0f;
    panel_driver->font_size.x = 1.0f;
    panel_driver->font_size.y = 1.0f;
    panel_driver->bg_color.a = 0;
    panel_driver->text_color.a = 0;
    panel_driver->render_callback = progression_panel_render;
}

static void panel_driver_close(void)
{
    if (panel_driver != NULL) {
        HSD_SisLib_803A5CC4(panel_driver);
        panel_driver = NULL;
    }
}

/* ------------------------------------------------------------------------- */
/* Text layer                                                                */
/* ------------------------------------------------------------------------- */

static void draw_route_text(HSD_Text* text)
{
    static const char* names[6] = {
        "CLEAR", "NEXT", "ELITE", "MATCH 4", "SHOP", "BOSS"
    };
    static const float x[6] = {
        58.0f, 158.0f, 254.0f, 344.0f, 454.0f, 550.0f
    };
    const RogueRoute* route = &g_rogue_run.route;
    int i;

    for (i = 0; i < 6; ++i) {
        bool complete = false;
        bool active = false;
        GXColor* color = &ui_muted;

        if (i < ROGUE_ROUTE_ROUNDS)
            complete = route->rounds[i].selected >= 0;

        if (target_act_floor < ROGUE_FLOORS_PER_ACT)
            active = i == target_act_floor - 1;
        else
            active = i == 4;

        if (active)
            color = &ui_gold;
        else if (complete)
            color = &ui_white;

        ui_entry_raw(text, x[i], 42.0f, .34f, color, names[i]);
    }

    ui_title(text, 250.0f, 64.0f, .45f, &ui_white, "ACT");
    ui_entryf(text, 287.0f, 64.0f, .45f, &ui_white,
              "%d", route->act);
    ui_entry_raw(text, 309.0f, 64.0f, .45f, &ui_muted, "-");
    ui_title(text, 330.0f, 64.0f, .45f, &ui_white, "FLOOR");
    ui_entryf(text, 387.0f, 64.0f, .45f, &ui_white,
              "%d", target_floor);
}

static void draw_phase_text(HSD_Text* text)
{
    const char* phase;

    if (has_reward && !upgrade_chosen)
        phase = "CHOOSE UPGRADE";
    else
        phase = has_reward ? "CHOOSE NEXT FIGHT" : "CHOOSE FIRST FIGHT";

    ui_title(text, has_reward ? 246.0f : 232.0f,
             92.0f, .56f, &ui_gold, phase);

    if (has_reward && upgrade_chosen && upgrade_taken >= 0) {
        ui_entryf(text, 248.0f, 110.0f, .31f, &ui_muted,
                  "LOCKED: %s",
                  g_rogue_run.current_rewards[upgrade_taken].name);
    }
}

static void draw_reward_text(HSD_Text* text)
{
    static const float x[3] = { 39.0f, 237.0f, 435.0f };
    int i;

    for (i = 0; i < 3; ++i) {
        RogueReward* reward = &g_rogue_run.current_rewards[i];
        GXColor* accent = upgrade_cursor == i ? &ui_gold : &ui_white;
        char meta[80];
        char detail[180];
        char line1[64];
        char line2[64];

        Rogue_DescribeReward(reward, detail, sizeof(detail));
        split_description(detail,
                          line1, sizeof(line1),
                          line2, sizeof(line2),
                          26);

        snprintf(meta, sizeof(meta), "%s / %s",
                 reward_category(reward),
                 Rogue_RarityName(reward->rarity));

        ui_title(text, x[i] + 7.0f, 144.0f,
                 .47f, accent, reward_icon_letter(reward));

        ui_title(text, x[i] + 37.0f, 142.0f,
                 .49f, accent, reward->name);

        ui_entry_raw(text, x[i] + 37.0f, 160.0f,
                     .30f, &ui_muted, meta);

        ui_entry_raw(text, x[i], 184.0f,
                     .36f, &ui_white, line1);

        if (line2[0]) {
            ui_entry_raw(text, x[i], 199.0f,
                         .36f, &ui_white, line2);
        }
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

        ui_title(text, 116.0f, 337.0f, .50f,
                 upgrade_chosen ? &ui_gold : &ui_white,
                 "SHOP / REST AREA");
        ui_entryf(text, 320.0f, 338.0f, .42f, &ui_muted,
                  "BOSS: %s", boss);
        ui_entry_raw(text, 220.0f, 360.0f, .34f, &ui_muted,
                     upgrade_chosen ?
                     "UPGRADE LOCKED IN - CONTINUING" :
                     "PICK AN UPGRADE FIRST");
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
        GXColor* title_color = selected ? &ui_gold : side;
        char name[96];
        char meta[120];
        char detail[120];

        encounter_name(name, sizeof(name), encounter);

        snprintf(meta, sizeof(meta), "%s / %s",
                 RogueRoute_StageName(encounter->stage),
                 RogueRoute_TypeName(encounter->type));

        if (!upgrade_chosen) {
            snprintf(detail, sizeof(detail), "PICK UPGRADE FIRST");
        } else if (encounter->modifier && *encounter->modifier) {
            snprintf(detail, sizeof(detail), "%s", encounter->modifier);
        } else if (encounter->enemy_count > 0) {
            snprintf(detail, sizeof(detail), "%d STOCK%s",
                     encounter->enemies[0].stocks,
                     encounter->enemies[0].stocks == 1 ? "" : "S");
        } else {
            snprintf(detail, sizeof(detail), "STANDARD");
        }

        ui_title(text, x, 337.0f, .52f, title_color, name);
        ui_entry_raw(text, x, 356.0f, .32f, side, meta);
        ui_entry_raw(text, x, 369.0f, .29f, &ui_muted, detail);
    }
}

static void draw_build_strip_text(HSD_Text* text)
{
    static const float key_x[4] = {
        44.0f, 190.0f, 336.0f, 482.0f
    };
    static const float name_x[4] = {
        70.0f, 216.0f, 362.0f, 508.0f
    };
    static const char* keys[4] = { "N", "S", "U", "D" };
    const RogueStats* stats = &g_rogue_run.stats;
    const char* controls;
    int score =
        g_rogue_run.wins * 10000 +
        g_rogue_run.currency * 100;
    int i;

    ui_title(text, 188.0f, 394.0f, .47f,
             &ui_white,
             "CURRENT CHARACTER BUILD / UPGRADES");

    for (i = 0; i < 4; ++i) {
        ui_entry_raw(text, key_x[i], 419.0f,
                     .37f, &ui_gold, keys[i]);
        ui_entryf(text, name_x[i], 419.0f,
                  .34f, &ui_white, "%.14s",
                  ability_name(i));
    }

    if (has_reward) {
        ui_entryf(text, 116.0f, 448.0f, .31f, &ui_muted,
                  "GOLD +%d  |  TOTAL %d  |  SCORE %d  |  DMG %.0f%%  |  DEF %.0f%%",
                  gold_gain,
                  g_rogue_run.currency,
                  score,
                  stats->damage_dealt * 100.0f,
                  stats->damage_received * 100.0f);
    } else {
        ui_entryf(text, 145.0f, 448.0f, .31f, &ui_muted,
                  "GOLD %d  |  SCORE %d  |  DMG %.0f%%  |  DEF %.0f%%",
                  g_rogue_run.currency,
                  score,
                  stats->damage_dealt * 100.0f,
                  stats->damage_received * 100.0f);
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

    ui_entry_raw(text, 186.0f, 466.0f, .34f,
                 confirm_timer > 0 ? &ui_gold : &ui_white,
                 controls);
}

static void draw_full_build_text(HSD_Text* text)
{
    const RogueStats* stats = &g_rogue_run.stats;

    ui_title(text, 190.0f, 69.0f, .62f,
             &ui_gold, "CURRENT CHARACTER BUILD");

    ui_entry_raw(text, 108.0f, 130.0f, .32f,
                 &ui_muted, "NEUTRAL");
    ui_title(text, 108.0f, 148.0f, .47f,
             &ui_white, ability_name(ROGUE_ABILITY_NEUTRAL));

    ui_entry_raw(text, 358.0f, 130.0f, .32f,
                 &ui_muted, "SIDE");
    ui_title(text, 358.0f, 148.0f, .47f,
             &ui_white, ability_name(ROGUE_ABILITY_SIDE));

    ui_entry_raw(text, 108.0f, 206.0f, .32f,
                 &ui_muted, "UP");
    ui_title(text, 108.0f, 224.0f, .47f,
             &ui_white, ability_name(ROGUE_ABILITY_UP));

    ui_entry_raw(text, 358.0f, 206.0f, .32f,
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

    ui_clear();

    text = ui_text_group();
    if (text == NULL)
        return;

    if (build_open) {
        draw_full_build_text(text);
        return;
    }

    draw_route_text(text);
    draw_phase_text(text);

    if (has_reward && !upgrade_chosen)
        draw_reward_text(text);

    draw_fight_text(text);
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
    panel_driver = NULL;
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
    panel_driver_close();
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

    /*
     * Create the custom-panel host first so its callback renders beneath all
     * Rogue text objects created by draw_progression().
     */
    panel_driver_open();
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
