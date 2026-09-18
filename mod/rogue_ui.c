#include "rogue_history.h"
#include "rogue.h"
#include "rogue_camp.h"
#include "rogue_format.h"
#include "rogue_ui.h"
#include "rogue_state.h"
#include <melee/mn/mnmain.h>
#include <melee/mn/inlines.h>
#include <melee/ft/types.h>
#include <melee/pl/player.h>
#include <melee/cm/camera.h>
#include <sysdolphin/baselib/cobj.h>
#include <sysdolphin/baselib/gobj.h>
#include <sysdolphin/baselib/gobjplink.h>
#include <sysdolphin/baselib/sislib.h>
#include <sysdolphin/baselib/controller.h>
#include <dolphin/gx.h>
#include <melee/lb/lbaudio_ax.h>
#include <melee/if/ifall.h>
#include <melee/if/ifstock.h>
#include <sysdolphin/baselib/jobj.h>
#include <stdarg.h>
#include <stdio.h>
#include <printf.h>
#include <string.h>
static HSD_Text* lines[128];
static unsigned line_count;
static int overlay = 1, overlay_canvas, overlay_sis;
extern u8 mn_804D6BB4;
static GXColor ui_white = {237, 239, 250, 255};
static GXColor ui_gold = {255, 200, 0, 255};
static GXColor ui_muted = {175, 187, 215, 255};
static GXColor ui_dark = {9, 15, 35, 230};
static GXColor ui_black = {18, 21, 34, 255};
static GXColor ui_red = {198, 28, 20, 255};
static GXColor ui_orange = {255, 104, 0, 255};
static GXColor ui_screen = {0, 0, 0, 214};
static GXColor ui_panel = {18, 22, 38, 210};


static int ui_format(char* out, unsigned int size, const char* fmt, ...)
{
    int result;
    va_list ap;
    va_start(ap, fmt); result = vsnprintf(out, size, fmt, ap); va_end(ap);
    return result;
}
static void ui_begin(void)
{
    unsigned int i;
    for (i = 0; i < line_count; ++i) HSD_SisLib_803A5CC4(lines[i]);
    line_count = 0;
}
/* SIS consumes Shift-JIS pairs for punctuation absent from its ASCII parser.
 * Leave supported ASCII alone, and explicitly encode the other symbols. */
static void ui_encode(char* out, const char* in)
{
    unsigned int n = 0;
    unsigned char lo;
    while (*in && n < 120) {
        lo = 0;
        switch (*in) {
        case '+': lo = 0x7B; break; case '%': lo = 0x93; break;
        case '/': lo = 0x5E; break; case '[': lo = 0x6D; break;
        case ']': lo = 0x6E; break; case '>': lo = 0x84; break;
        case '<': lo = 0x83; break; case '|': lo = 0x62; break;
        case '!': lo = 0x49; break; case '?': lo = 0x48; break;
        case '=': lo = 0x81; break; case '&': lo = 0x95; break;
        case ';': lo = 0x47; break; case '(': lo = 0x69; break;
        case ')': lo = 0x6A; break;
        }
        if (lo) { out[n++] = (char)0x81; out[n++] = (char)lo; }
        else out[n++] = *in;
        ++in;
    }
    out[n] = 0;
}
static HSD_Text* ui_object(float x, float y, float size, GXColor color)
{
    HSD_Text* t = HSD_SisLib_803A6754(overlay_sis, overlay ? overlay_canvas : mn_804D6BB4);
    lines[line_count++] = t;
    t->pos_x = overlay ? 320 + x * 16 : x;
    t->pos_y = overlay ? 240 + y * 16 : y;
    t->pos_z = overlay ? 0 : 17.0f;
    t->font_size.x = t->font_size.y = overlay ? size * 16 : size;
    t->default_kerning = 1; t->text_color = color;
    return t;
}
static void ui_at(float x, float y, float size, GXColor color, const char* fmt, ...)
{
    char raw[256], encoded[128];
    va_list ap;
    HSD_Text* t;
    va_start(ap, fmt); vsnprintf(raw, sizeof(raw), fmt, ap); va_end(ap);
    ui_encode(encoded, raw);
    t = ui_object(x, y, size, color);
    HSD_SisLib_803A6B98(t, 0, 0, "%s", encoded);
}
static void ui_card(float y, float height, int selected, const char* title,
                    const char* detail, const char* extra)
{
    char encoded[128];
    HSD_Text* t = ui_object(-13.6f, y, 0.022f, selected ? ui_black : ui_white);
    int entry;
    t->bg_color = selected ? ui_gold : ui_dark;
    t->box_size_x = 27.2f / 0.022f; t->box_size_y = height / 0.022f;
    ui_encode(encoded, title);
    HSD_SisLib_803A6B98(t, 22.0f, 9.0f, "%s", encoded);
    if (detail && *detail) {
        ui_encode(encoded, detail);
        entry = HSD_SisLib_803A6B98(t, 22.0f, 54.0f, "%s", encoded);
        HSD_SisLib_803A7548(t, entry, 0.75f, 0.75f);
    }
    if (extra && *extra) {
        ui_encode(encoded, extra);
        entry = HSD_SisLib_803A6B98(t, 22.0f, 90.0f, "%s", encoded);
        HSD_SisLib_803A7548(t, entry, 0.75f, 0.75f);
    }
}

static HSD_Text* ui_panel_box(float x, float y, float width, float height,
                              GXColor bg)
{
    HSD_Text* t = ui_object(x, y, .010f, ui_white);
    t->bg_color = bg;
    t->box_size_x = width / .010f;
    t->box_size_y = height / .010f;
    HSD_SisLib_803A6B98(t, 0, 0, " ");
    return t;
}

static void ui_backdrop(void)
{
    ui_panel_box(-20.0f, -15.0f, 40.0f, 30.0f, ui_screen);
}

static void ui_rule(float x, float y, float width, GXColor color)
{
    ui_panel_box(x, y, width, .12f, color);
}

static void ui_tile(float x, float y, float width, float height, int selected,
                    GXColor accent, const char* title, const char* subtitle,
                    const char* tag)
{
    GXColor bg = selected ? accent : ui_panel;
    GXColor fg = selected ? ui_black : ui_white;
    GXColor sub = selected ? ui_black : ui_muted;

    ui_panel_box(x, y, width, height, bg);
    if (title && *title) {
        float title_size = strlen(title) > 18 ? .0165f :
                           strlen(title) > 14 ? .0185f : .021f;
        ui_at(x + .55f, y + .55f, title_size, fg, "%s", title);
    }
    if (subtitle && *subtitle)
        ui_at(x + .55f, y + 2.15f, .0145f, sub, "%s", subtitle);
    if (tag && *tag)
        ui_at(x + .55f, y + height - 1.35f, .0135f, sub, "%s", tag);
}

static void encounterOpponent(char* out, unsigned size,
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

static const char* rewardCategory(const RogueReward* reward)
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
    default:
        return "BOOST";
    }
}

static int encounterGold(const RogueEncounter* encounter)
{
    return encounter->type == ROGUE_ENCOUNTER_BOSS ? 100 :
           encounter->type == ROGUE_ENCOUNTER_ELITE ? 60 : 30;
}


#define ROGUE_UI_NATIVE_ICONS 10
static HSD_GObj* native_icons[ROGUE_UI_NATIVE_ICONS];
static int native_icon_count;
static bool native_ifall_owned;

static void ui_clear_native_icons(void)
{
    int i;
    for (i = 0; i < native_icon_count; ++i) {
        if (native_icons[i] != NULL)
            HSD_GObjFree(native_icons[i]);
        native_icons[i] = NULL;
    }
    native_icon_count = 0;
}

static bool ui_ensure_native_icons(void)
{
    HSD_Archive** archive;

    if (ifAll_GetHUDGObj() != NULL)
        return true;

    archive = ifAll_GetArchive();
    if (archive == NULL)
        return false;

    /* Adventure intro does not normally own the battle HUD archive.
     * Load native IfAll resources temporarily for fighter portrait icons. */
    ifAll_802F390C();
    native_ifall_owned = true;
    return ifAll_GetHUDGObj() != NULL;
}

static HSD_GObj* ui_fighter_icon(CharacterKind kind, int costume,
                                 float x, float y, float scale)
{
    HSD_GObj* gobj;
    HSD_JObj* jobj;
    Vec3 size;

    if (native_icon_count >= ROGUE_UI_NATIVE_ICONS)
        return NULL;
    if (!ui_ensure_native_icons())
        return NULL;

    gobj = ifStock_802F96D0(kind, costume, x, y);
    if (gobj == NULL)
        return NULL;

    jobj = gobj->hsd_obj;
    if (jobj != NULL) {
        size.x = size.y = size.z = scale;
        HSD_JObjSetScale(jobj, &size);
    }

    native_icons[native_icon_count++] = gobj;
    return gobj;
}

static void ui_wrapped_at(float x, float y, float size, GXColor color,
                          const char* text, int width, int max_lines)
{
    const char* p = text;
    int row = 0;

    while (*p && row < max_lines) {
        char line[96];
        int remaining = (int) strlen(p);
        int count = remaining > width ? width : remaining;

        if (remaining > width) {
            while (count > 0 && p[count] != ' ')
                --count;
            if (count <= 0)
                count = width;
        }

        if (count >= (int) sizeof(line))
            count = sizeof(line) - 1;

        memcpy(line, p, count);
        line[count] = 0;
        ui_at(x, y + row * 1.12f, size, color, "%s", line);

        p += count;
        while (*p == ' ')
            ++p;
        ++row;
    }
}

static int cursor, page, delay, camp_zone = -2, camp_branch;
static bool ready, inspect, history_open, hud_ready;
static int history_cursor;
static const RogueHistoryEntry* viewed_record;
static HSD_Text* markers[5];
static float camp_y;
void RogueUI_Clear(void)
{
    ui_clear_native_icons();
    ui_begin();
    ready = false;
    if (native_ifall_owned) {
        ifAll_802F3A64();
        native_ifall_owned = false;
    }
}
void RogueUI_Reset(void)
{
    /* Called after scene teardown: SIS has already released its scene objects. */
    line_count = 0; ready = false; cursor = page = delay = camp_branch = 0;
    overlay_sis = 0;
    camp_zone = -2; inspect = false;
    RogueCamp_Reset();
    history_open=false;history_cursor=0;viewed_record=NULL;
    hud_ready = false;
    native_icon_count = 0;
    native_ifall_owned = false;
    memset(native_icons, 0, sizeof(native_icons));
    memset(markers, 0, sizeof(markers));
}
static void openCanvas(void)
{
    if (ready) return;
    overlay_canvas = HSD_SisLib_803A611C(overlay_sis, NULL, 9, 13, 0, 20, 0, 15);
    ready = true;
}

void RogueUI_HudFrame(void)
{
    const RogueEncounter* encounter;
    HSD_Text* shadow;
    HSD_Text* panel;
    HSD_Text* accent;
    char raw[128], encoded[128];
    int fight;
    float height;

    if (hud_ready || !Rogue_IsActive() ||
        g_rogue_run.phase != ROGUE_PHASE_ENCOUNTER)
        return;

    openCanvas();
    encounter = &g_rogue_run.current_encounter;
    fight = (encounter->act - 1) * ROGUE_FLOORS_PER_ACT +
            encounter->act_floor;
    height = encounter->modifier ? 3.15f : 2.55f;

    shadow = ui_object(-18.92f, -13.86f, .0112f, ui_black);
    shadow->bg_color = ui_black;
    shadow->box_size_x = 10.55f / .0112f;
    shadow->box_size_y = (height + .25f) / .0112f;

    panel = ui_object(-18.78f, -13.72f, .0112f, ui_white);
    panel->bg_color = ui_dark;
    panel->box_size_x = 10.25f / .0112f;
    panel->box_size_y = height / .0112f;

    accent = ui_object(-18.78f, -13.72f, .0108f, ui_black);
    accent->bg_color = ui_gold;
    accent->box_size_x = 2.35f / .0108f;
    accent->box_size_y = .92f / .0108f;
    ui_encode(encoded, "ROGUE");
    HSD_SisLib_803A6B98(accent, 10.0f, 6.0f, "%s", encoded);

    snprintf(raw, sizeof(raw), "FIGHT %d/%d", fight, ROGUE_RUN_ENCOUNTERS);
    ui_at(-16.10f, -13.15f, .0105f, ui_white, "%s", raw);

    ui_at(-18.42f, -12.15f, .0094f, ui_muted,
          "ACT %d-%d   G%d   %s",
          encounter->act, encounter->act_floor, g_rogue_run.currency,
          RogueRoute_TypeName(encounter->type));

    if (encounter->modifier)
        ui_at(-18.42f, -11.30f, .0088f, ui_gold, "%.30s",
              encounter->modifier);

    hud_ready = true;
}

static void buildPage(void)
{
    const RogueStats* s = viewed_record ? &viewed_record->stats : &g_rogue_run.stats;
    ui_backdrop();
    ui_panel_box(-14.0f, -11.0f, 28.0f, 21.5f, ui_panel);
    ui_at(-13.2f, -10, .035f, ui_gold, "CURRENT BUILD");
    ui_at(-13.2f, -7.6f, .019f, ui_muted, "Page %d / 4     Seed %u", page+1, viewed_record?viewed_record->seed:g_rogue_run.seed);
    if (page == 0) {
        ui_at(-13, -5, .023f, ui_white, "Damage %.0f%%   Received %.0f%%", s->damage_dealt*100, s->damage_received*100);
        ui_at(-13, -2.5f, .023f, ui_white, "Run %.0f%%   Shield %.0f%%", s->run_speed*100, s->shield_health*100);
        ui_at(-13, 0, .022f, ui_white, "Air +%.0f%%   Jump +%.0f%%   Jumps +%d", s->air_control_bonus*100,s->jump_height_bonus*100,s->extra_jumps);
        ui_at(-13, 2.5f, .022f, ui_white, "Fast fall +%.0f%%   Slide %.2fx",s->fast_fall_bonus*100,1+s->slide_bonus);
        ui_at(-13, 5, .022f, ui_white, "Shield regen +%.0f%%   Stun -%.0f%%",s->shield_regen_bonus*100,s->shield_stun_reduction*100);
    } else if (page == 1) {
        for(int i=0;i<4;i++) {
            RogueAbilityID id=viewed_record?viewed_record->ability[i]:g_rogue_run.ability[i];
            if(id==ROGUE_ABILITY_NATIVE) id=Rogue_AbilityForOpponent(viewed_record?viewed_record->character:g_rogue_run.player_kind,i);
            const RogueAbilityDefinition* d=Rogue_GetAbility(id);
            const char* slots[]={"Neutral","Side","Up","Down"};
            ui_at(-13,-5+i*3,.022f,ui_white,"%s: %s",slots[i],d?d->name:"Native");
        }
    } else if(page==2) {
        ui_at(-13,-5,.023f,ui_white,"Knockback +%.0f%%   Resist %.0f%%",s->knockback_dealt_bonus*100,s->knockback_resistance*100);
        ui_at(-13,-2.5f,.022f,ui_white,"Executioner +%.0f%%   Aerial +%.0f%%",s->executioner_bonus*100,s->aerial_damage_bonus*100);
        ui_at(-13,0,.022f,ui_white,"Smash +%.0f%%   Armor %.0f",s->smash_damage_bonus*100,s->smash_armor);
        ui_at(-13,2.5f,.022f,ui_white,"Combo +%.1f%%   Momentum +%.1f%%",s->combo_bonus*100,s->momentum_bonus*100);
        ui_at(-13,5,.022f,ui_white,"Parry heal %.0f%%   KO heal %.0f%%",s->parry_heal,s->ko_heal);
    } else {
        ui_at(-13,-5,.022f,ui_white,"Static: %s   Mirror: %s",s->static_effect?"ON":"OFF",s->mirror?"ON":"OFF");
        ui_at(-13,-2.5f,.022f,ui_white,"Second Shell: %s",s->second_shell?"Once per fight":"OFF");
        ui_at(-13,0,.022f,ui_white,"Last Stand: %s",s->last_stand?"Once per fight":"OFF");
        ui_at(-13,2.5f,.022f,ui_white,"Piercing shots: %s",s->piercing_shots?"Once per projectile":"OFF");
        ui_at(-13,5,.022f,ui_white,"Hitstun -%.0f%%",s->hitstun_reduction*100);
    }
    ui_at(-13,9,.018f,ui_white,"L / R: page     B: return");
}
static void drawHistory(void)
{
    int count;
    int i;

    ui_backdrop();
    ui_at(-13.5f, -11.8f, .038f, ui_gold, "RUN HISTORY");
    ui_rule(-13.5f, -9.9f, 27.0f, ui_gold);

    count = RogueHistory_Count();
    if (!count)
        ui_at(-13.2f, -5.0f, .022f, ui_white,
              "No completed runs yet.");

    for (i = 0; i < 3; ++i) {
        int index = (history_cursor / 3) * 3 + i;
        const RogueHistoryEntry* e = RogueHistory_Get(index);
        char title[100], detail[100];
        if (!e) break;
        snprintf(title, sizeof(title), "%d. %s  -  %d / 15 wins",
                 index + 1, e->complete ? "Victory" : "Defeat", e->wins);
        snprintf(detail, sizeof(detail), "Seed %u   Gold %d",
                 e->seed, e->currency);
        ui_card(-6 + i * 4.2f, 3.7f, index == history_cursor,
                title, detail, "A: inspect final build");
    }

    ui_at(-13.2f, 9.5f, .016f, ui_white,
          "Control Stick: choose     A: inspect     B: return");
    if (RogueHistory_Status() < 0)
        ui_at(-13.2f, 11.2f, .0135f, ui_gold,
              "Memory card unavailable. Recent history is in memory.");
}

static void drawRunEnd(void)
{
    static const char* names[] = {
        "NEW RUN", "REPLAY SEED", "HISTORY", "1-P MENU"
    };
    static const char* descriptions[] = {
        "Choose a fighter and begin a new run.",
        "Choose a fighter using this same seed.",
        "Inspect recent runs and their final builds.",
        "Return to 1-P Mode."
    };
    GXColor accent = g_rogue_run.phase == ROGUE_PHASE_COMPLETE ?
                     ui_gold : ui_red;
    int i;

    ui_backdrop();

    ui_at(-13.5f, -11.8f, .048f, accent,
          g_rogue_run.phase == ROGUE_PHASE_COMPLETE ?
          "RUN COMPLETE" : "RUN OVER");
    if (g_rogue_run.phase != ROGUE_PHASE_COMPLETE)
        ui_at(6.9f, -10.8f, .020f, ui_red, "CONTINUE?");

    ui_rule(-13.5f, -9.6f, 27.0f, accent);

    ui_clear_native_icons();
    ui_panel_box(-13.5f, -8.4f, 26.65f, 5.15f, ui_panel);
    ui_fighter_icon(g_rogue_run.player_kind, g_rogue_run.player_costume,
                    10.9f, -6.1f, 1.9f);
    ui_at(-12.7f, -7.6f, .014f, ui_muted, "RUN SUMMARY");
    ui_at(-12.7f, -5.95f, .021f, ui_white,
          "ACT %d    FLOOR %d / 5    WINS %d / 15",
          g_rogue_run.current_encounter.act,
          g_rogue_run.current_encounter.act_floor,
          g_rogue_run.wins);
    ui_at(-12.7f, -4.30f, .018f, ui_gold,
          "GOLD %d     SEED %u",
          g_rogue_run.currency, g_rogue_run.seed);

    for (i = 0; i < 4; ++i) {
        ui_tile(-13.5f + i * 6.75f, -1.8f, 6.05f, 3.6f,
                cursor == i, accent, names[i], "", "");
    }

    ui_panel_box(-13.5f, 2.55f, 26.65f, 3.25f, ui_panel);
    ui_at(-12.7f, 3.45f, .016f, ui_white, "%s",
          descriptions[cursor]);

    ui_at(-13.2f, 8.4f, .016f, ui_white,
          "Stick: choose     A: confirm     B: build");
}

static void draw(void)
{
    ui_begin();

    if (inspect) {
        buildPage();
        return;
    }

    if (history_open) {
        drawHistory();
        return;
    }

    drawRunEnd();
}

void RogueUI_OpenResults(void)
{
    /*
     * GmRegClr has already created native SIS slot 0 / canvas 0.
     * Remove Rogue's fight HUD strings, then attach reward text directly to
     * that existing canvas so SCORE and the native result geometry remain
     * completely Melee-owned.
     */
    RogueUI_Clear();
    overlay_sis = 0;
    overlay_canvas = 0;
    ready = true;

    cursor = page = delay = 0;
    inspect = false;
    draw();
}

int RogueUI_Frame(void)
{
    u32 input;
    int count;

    if (++delay < 30)
        return ROGUE_UI_WAIT;

    input = mn_80229624(Rogue_ControllerPort());

    if (input & MenuInput_Back) {
        if (inspect) {
            inspect = false;
            viewed_record = NULL;
        } else if (history_open) {
            history_open = false;
        } else {
            inspect = true;
        }
        sfxBack();
        draw();
        return ROGUE_UI_WAIT;
    }

    if (inspect) {
        if (input & (MenuInput_Left | MenuInput_LTrigger)) {
            page = (page + 3) % 4;
            draw();
            sfxMove();
        }
        if (input & (MenuInput_Right | MenuInput_RTrigger)) {
            page = (page + 1) % 4;
            draw();
            sfxMove();
        }
        return ROGUE_UI_WAIT;
    }

    if (history_open) {
        int history_count = RogueHistory_Count();
        if (history_count && (input & MenuInput_Up)) {
            history_cursor = (history_cursor + history_count - 1) %
                             history_count;
            draw();
            sfxMove();
        }
        if (history_count && (input & MenuInput_Down)) {
            history_cursor = (history_cursor + 1) % history_count;
            draw();
            sfxMove();
        }
        if (history_count && (input & MenuInput_Confirm)) {
            viewed_record = RogueHistory_Get(history_cursor);
            inspect = true;
            page = 0;
            draw();
            sfxForward();
        }
        return ROGUE_UI_WAIT;
    }

    count = 4;

    if (input & (MenuInput_Left | MenuInput_Up)) {
        cursor = (cursor + count - 1) % count;
        draw();
        sfxMove();
    }
    if (input & (MenuInput_Right | MenuInput_Down)) {
        cursor = (cursor + 1) % count;
        draw();
        sfxMove();
    }

    if (input & (MenuInput_Confirm | MenuInput_StartButton)) {
        if (cursor == 2) {
            history_open = true;
            history_cursor = 0;
            draw();
            sfxForward();
            return ROGUE_UI_WAIT;
        }

        sfxForward();
        return cursor == 0 ? ROGUE_UI_NEW :
               cursor == 1 ? ROGUE_UI_REPLAY :
                             ROGUE_UI_EXIT;
    }

    return ROGUE_UI_WAIT;
}

/* Rogue Bracket: route + post-fight upgrade + next-match selection.
 * This still runs on the original working Rest Area-backed route scene. */
static int route_cursor;
static int route_reward_cursor;
static int route_reward_taken = -1;
static int route_confirm;
static int route_locked = -1;
static bool route_reward_mode;
static bool route_had_reward;

static void routeNode(float x, float y, float width,
                      int active, int complete,
                      const char* label, const char* detail)
{
    GXColor bg = active ? ui_gold :
                 complete ? ui_panel : ui_black;
    GXColor fg = active ? ui_black : ui_white;

    ui_panel_box(x, y, width, 1.35f, bg);
    ui_at(x + .30f, y + .32f, .0115f, fg, "%s", label);
    if (detail && *detail)
        ui_at(x + .05f, y + 1.55f, .0085f,
              complete ? ui_muted : ui_white, "%.12s", detail);
}

static void routeRewardBox(float x, RogueReward* reward,
                           int selected, int chosen)
{
    char detail[160];
    char meta[72];
    GXColor bg = selected ? ui_gold : ui_panel;
    GXColor fg = selected ? ui_black : ui_white;
    GXColor sub = selected ? ui_black : ui_muted;
    float title_size;

    Rogue_DescribeReward(reward, detail, sizeof(detail));
    snprintf(meta, sizeof(meta), "%s   %s%s",
             rewardCategory(reward),
             Rogue_RarityName(reward->rarity),
             chosen ? "   SELECTED" : "");

    ui_panel_box(x, -4.45f, 8.55f, 3.45f, bg);

    title_size = strlen(reward->name) > 18 ? .0135f :
                 strlen(reward->name) > 14 ? .0150f : .0170f;
    ui_at(x + .38f, -4.05f, title_size, fg, "%s", reward->name);
    ui_at(x + .38f, -2.92f, .0088f, sub, "%s", meta);
    ui_wrapped_at(x + .38f, -2.05f, .0084f, fg,
                  detail, 25, 1);
}

static void routeDrawRewards(void)
{
    int i;

    if (!route_had_reward) {
        ui_at(-13.6f, -5.25f, .0140f, ui_muted, "UPGRADES");
        ui_at(-13.05f, -3.15f, .0120f, ui_muted,
              "WIN A MATCH TO EARN AN UPGRADE");
        return;
    }

    ui_at(-13.6f, -5.25f, .0145f,
          route_reward_mode ? ui_gold : ui_muted,
          route_reward_mode ? "CHOOSE UPGRADE" : "UPGRADE SELECTED");

    for (i = 0; i < 3; ++i) {
        int selected = route_reward_mode
                           ? route_reward_cursor == i
                           : route_reward_taken == i;
        int chosen = !route_reward_mode && route_reward_taken == i;

        routeRewardBox(-13.6f + i * 9.15f,
                       &g_rogue_run.current_rewards[i],
                       selected, chosen);
    }
}

static void routeDraw(void)
{
    const RogueRoute* route = &g_rogue_run.route;
    const RogueRouteRound* current = RogueRoute_Current(route);
    static const float node_x[6] = {
        -14.8f, -9.25f, -3.70f, 1.85f, 7.40f, 12.95f
    };
    static const char* node_name[6] = {
        "MATCH 1", "MATCH 2", "ELITE", "MATCH 4", "SHOP", "BOSS"
    };
    const char* player = RogueRoute_CharacterName(g_rogue_run.player_kind);
    int i;

    ui_begin();
    ui_backdrop();
    ui_clear_native_icons();

    ui_at(-14.6f, -12.2f, .036f, ui_gold, "ROGUE ROUTE");
    ui_at(9.1f, -11.5f, .018f, ui_white, "ACT %d", route->act);
    ui_at(-14.5f, -9.55f, .012f, ui_muted,
          "ALL-STAR PROGRESSION");

    for (i = 0; i < 6; ++i) {
        char label[32];
        const char* detail = node_name[i];
        int round_index = i < 4 ? i : -1;
        int complete = round_index >= 0 &&
                       route->rounds[round_index].selected >= 0;
        int active = round_index >= 0 &&
                     round_index == route->current_round && !complete;

        if (i == 4) {
            snprintf(label, sizeof(label), "SHOP");
            detail = "REST AREA";
        } else if (i == 5) {
            snprintf(label, sizeof(label), "BOSS");
            detail = RogueRoute_CharacterName(route->boss.enemy_kind);
        } else if (complete) {
            const RogueEncounter* picked =
                &route->rounds[round_index]
                    .choices[route->rounds[round_index].selected];
            snprintf(label, sizeof(label), "CLEAR");
            detail = RogueRoute_CharacterName(picked->enemy_kind);
        } else if (active) {
            snprintf(label, sizeof(label), "NEXT");
            detail = node_name[i];
        } else {
            snprintf(label, sizeof(label), "%s", node_name[i]);
            detail = round_index == 2 ? "ELITE" : "PENDING";
        }

        routeNode(node_x[i], -7.9f, 3.55f,
                  active, complete, label, detail);

        if (i < 5)
            ui_rule(node_x[i] + 3.58f, -7.25f, 1.95f,
                    complete ? ui_red : ui_muted);
    }

    /* Reward row sits between route track and next-match choices. */
    routeDrawRewards();

    if (route_reward_mode) {
        ui_at(-13.6f, -.15f, .0145f, ui_muted, "CHOOSE NEXT MATCH");
        ui_panel_box(-13.6f, .65f, 26.8f, 4.10f, ui_panel);
        ui_at(-7.65f, 2.45f, .0150f, ui_muted,
              "CHOOSE AN UPGRADE FIRST");
        ui_at(-13.2f, 8.75f, .016f, ui_white,
              "Left / Right: upgrade     A: select     B: build");
        return;
    }

    current = RogueRoute_Current(route);
    if (!current || !current->generated) {
        ui_at(-13.6f, -.15f, .0145f, ui_muted, "CHOOSE NEXT MATCH");
        ui_at(-13.2f, 2.15f, .022f, ui_white,
              "Preparing next match...");
        return;
    }

    ui_at(-13.6f, -.15f, .0145f, ui_muted, "CHOOSE NEXT MATCH");

    for (i = 0; i < ROGUE_ROUTE_CHOICES; ++i) {
        const RogueEncounter* encounter = &current->choices[i];
        char opponent[80], subtitle[100], tag[120], preview[100], stock[32];
        float center_x = -7.2f + i * 13.7f;
        int selected = route_locked >= 0 ? route_locked == i :
                       route_cursor == i;

        encounterOpponent(opponent, sizeof(opponent), encounter);
        snprintf(subtitle, sizeof(subtitle), "%s   %s",
                 RogueRoute_StageName(encounter->stage),
                 RogueRoute_TypeName(encounter->type));

        if (encounter->enemy_count > 0) {
            snprintf(stock, sizeof(stock), "%d STOCK%s",
                     encounter->enemies[0].stocks,
                     encounter->enemies[0].stocks == 1 ? "" : "S");
        } else {
            stock[0] = 0;
        }

        if (encounter->modifier && *encounter->modifier) {
            if (stock[0])
                snprintf(tag, sizeof(tag), "%s   %s",
                         encounter->modifier, stock);
            else
                snprintf(tag, sizeof(tag), "%s", encounter->modifier);
        } else if (stock[0]) {
            snprintf(tag, sizeof(tag), "%s", stock);
        } else {
            tag[0] = 0;
        }

        if (route_confirm > 0 && route_locked == i) {
            snprintf(preview, sizeof(preview), "VS %s", opponent);
            ui_tile(-13.6f + i * 13.85f, .65f, 12.95f, 4.10f,
                    true, ui_gold, player, preview, tag);

            ui_fighter_icon(g_rogue_run.player_kind,
                            g_rogue_run.player_costume,
                            center_x - 2.9f, 2.70f, 1.55f);
            ui_fighter_icon(encounter->enemy_kind,
                            encounter->enemy_count ?
                                encounter->enemies[0].costume : 0,
                            center_x + 2.9f, 2.70f, 1.55f);
            ui_at(center_x, 2.55f, .024f, ui_orange, "VS");
        } else {
            ui_tile(-13.6f + i * 13.85f, .65f, 12.95f, 4.10f,
                    selected, ui_gold, opponent, subtitle, tag);

            ui_fighter_icon(encounter->enemy_kind,
                            encounter->enemy_count ?
                                encounter->enemies[0].costume : 0,
                            center_x, 2.75f, 1.80f);
        }
    }

    ui_panel_box(-13.6f, 5.30f, 26.8f, 1.85f, ui_panel);
    ui_at(-12.9f, 5.80f, .0115f, ui_muted,
          "AFTER MATCH 4: SHOP / REST AREA");
    ui_at(2.4f, 5.75f, .0125f, ui_white, "FINAL: %s",
          route->boss.name ? route->boss.name : "Boss");

    if (route_confirm > 0)
        ui_at(4.75f, 8.75f, .016f, ui_gold,
              "MATCH SET  -  STARTING ENCOUNTER");
    else
        ui_at(-13.2f, 8.75f, .016f, ui_white,
              "Stick: choose     A: select     B: build");
}

void RogueUI_OpenRoute(void)
{
    openCanvas();
    route_had_reward =
        g_rogue_run.phase == ROGUE_PHASE_REWARD &&
        g_rogue_run.reward_pending;
    route_reward_mode = route_had_reward;
    route_reward_cursor = 0;
    route_reward_taken = -1;
    route_cursor = 0;
    route_confirm = 0;
    route_locked = -1;
    inspect = false;
    page = 0;
    delay = 30;
    routeDraw();
}

int RogueUI_RouteFrame(void)
{
    u32 input;

    if (route_confirm > 0) {
        --route_confirm;
        if (route_confirm == 0) {
            int choice = route_locked;
            route_locked = -1;
            return choice;
        }
        return -1;
    }

    input = mn_80229624(Rogue_ControllerPort());

    if (input & MenuInput_Back) {
        if (inspect) {
            inspect = false;
            routeDraw();
        } else {
            inspect = true;
            page = 0;
            ui_begin();
            buildPage();
        }
        sfxBack();
        return -1;
    }

    if (inspect) {
        if (input & (MenuInput_Left | MenuInput_LTrigger)) {
            page = (page + 3) % 4;
            ui_begin();
            buildPage();
            sfxMove();
        }
        if (input & (MenuInput_Right | MenuInput_RTrigger)) {
            page = (page + 1) % 4;
            ui_begin();
            buildPage();
            sfxMove();
        }
        return -1;
    }

    if (route_reward_mode) {
        if (input & (MenuInput_Left | MenuInput_Up)) {
            route_reward_cursor = (route_reward_cursor + 2) % 3;
            routeDraw();
            sfxMove();
        }
        if (input & (MenuInput_Right | MenuInput_Down)) {
            route_reward_cursor = (route_reward_cursor + 1) % 3;
            routeDraw();
            sfxMove();
        }

        if (input & (MenuInput_Confirm | MenuInput_StartButton)) {
            if (!Rogue_SelectReward(route_reward_cursor))
                return -1;

            route_reward_taken = route_reward_cursor;
            route_reward_mode = false;
            route_cursor = 0;
            route_locked = -1;
            route_confirm = 0;
            sfxForward();

            /* Match 4 reward prepares the boss; preserve shop-before-boss. */
            if (g_rogue_run.phase == ROGUE_PHASE_ENCOUNTER)
                return ROGUE_ROUTE_CHOICES;

            routeDraw();
        }
        return -1;
    }

    if (input & (MenuInput_Left | MenuInput_Right |
                 MenuInput_Up | MenuInput_Down)) {
        route_cursor ^= 1;
        routeDraw();
        sfxMove();
    }

    if (input & MenuInput_Confirm) {
        route_locked = route_cursor;
        route_confirm = 20;
        routeDraw();
        sfxForward();
    }

    return -1;
}

/* Camp signs are projected from world coordinates every frame. Walking and
 * jumping continue normally; only grounded contact with a zone permits A. */

static void campDraw(int zone)
{
    ui_begin();
    ui_at(-17,-13,.020f,ui_gold,"SHOP / REST AREA    Gold %d",g_rogue_run.currency);
    for(int i=0;i<5;i++) {
        const char* names[]={"SHOP 1","SHOP 2","REST","TRAIN","NEXT FIGHT"};
        HSD_Text* t=ui_object(0,0,.013f,i==zone?ui_gold:ui_white);
        t->default_alignment=1;
        /* World labels are glyphs only. A SIS background uses the default
         * text canvas bounds and would obscure the stage with large panels. */
        t->bg_color.a=0;
        if(i<2) HSD_SisLib_803A6B98(t,0,0,g_rogue_run.shop_sold[i]?"SOLD":"%d gold",g_rogue_run.shop_prices[i]);
        else HSD_SisLib_803A6B98(t,0,0,"%s",names[i]);
        markers[i]=t;
    }
    if(zone>=0) {
        char title[128],detail[256];
        if(zone<2) {
            RogueReward* r=&g_rogue_run.shop_rewards[zone];
            snprintf(title,sizeof(title),"%s  -  %d gold",r->name,g_rogue_run.shop_prices[zone]);
            Rogue_DescribeReward(r,detail,sizeof(detail));
            if(g_rogue_run.shop_sold[zone]) strcpy(detail,"SOLD");
            else if(camp_branch==2) strcpy(detail,"Rest or training already chosen.");
            else if(g_rogue_run.currency<g_rogue_run.shop_prices[zone]) strcpy(detail,"Not enough gold.");
        } else {
            const char* titles[]={"Reinforce shield","Movement training","Next encounter"};
            const char* details[]={"+15% shield capacity for this run.","+10% dash and run speed for this run.","Keep remaining gold and continue the run."};
            strcpy(title,titles[zone-2]);strcpy(detail,details[zone-2]);
            if(zone<4 && camp_branch) strcpy(detail,"Camp benefit already selected.");
        }
        /* Keep descriptions in the sky, clear of the fighter and native HUD.
         * Use separate text rows so SIS never truncates a long description. */
        ui_at(-17,-10.8f,.023f,ui_gold,"%s",title);
        const char* remaining=detail;
        int row=0;
        while(*remaining && row<4) {
            char text[72];
            int count=(int)strlen(remaining);
            if(count>60) {
                count=60;
                while(count>0 && remaining[count]!=' ') --count;
                if(!count) count=60;
            }
            memcpy(text,remaining,count);text[count]=0;
            ui_at(-17,-9.2f+row*1.15f,.017f,ui_white,"%s",text);
            remaining+=count;
            while(*remaining==' ') ++remaining;
            ++row;
        }
        ui_at(-17,-9.2f+row*1.15f,.016f,ui_gold,
              zone==4?"A: continue":"A: select while standing in this zone");
    } else ui_at(-17,-10.8f,.017f,ui_white,"A: choose a camp item or enter the glowing exit.");
}
bool RogueUI_CampFrame(void)
{
    HSD_GObj* entity=Player_GetEntity(0);
    Fighter* fp=entity?entity->user_data:NULL;
    if(!fp) return false;
    if(!ready) { openCanvas();RogueCamp_Create(); }
    int zone=RogueCamp_ZoneAt(fp->cur_pos.x,fp->cur_pos.y,fp->ground_or_air==GA_Ground);
    RogueCamp_Update(zone,camp_branch);
    if(zone!=camp_zone) { camp_zone=zone;campDraw(zone); }
    HSD_GObj* cam=Camera_80030A50();
    if(cam && cam->hsd_obj) {
        HSD_CObj* c=cam->hsd_obj;Mtx44 projection;float viewport[6]={0,0,640,480,0,1};
        int type=makeProjectionMtx(c,projection);
        float pv[7];pv[0]=type;pv[1]=projection[0][0];pv[3]=projection[1][1];
        pv[2]=projection[0][type==GX_PERSPECTIVE?2:3];
        pv[4]=projection[1][type==GX_PERSPECTIVE?2:3];
        pv[5]=projection[2][2];pv[6]=projection[2][3];
        for(int i=0;i<5;i++) {
            float x,y,z;
            const Vec3* position=RogueCamp_Position(i);
            GXProject(position->x,position->y+26,0,HSD_CObjGetViewingMtxPtr(c),pv,viewport,&x,&y,&z);
            if(markers[i]) { markers[i]->pos_x=x;markers[i]->pos_y=y; }
        }
    }
    if(zone>=0 && (HSD_PadMasterStatus[Rogue_ControllerPort()].trigger&HSD_PAD_A)) {
        bool changed=false;
        if(zone<2 && camp_branch!=2) {
            RoguePhase phase=g_rogue_run.phase;g_rogue_run.phase=ROGUE_PHASE_SHOP;
            changed=Rogue_BuySupply(zone);
            if(changed) camp_branch=1;else g_rogue_run.phase=phase;
        } else if(zone<4 && !camp_branch) {
            changed=Rogue_Rest(zone-2);if(changed) camp_branch=2;
        } else if(zone==4) { Rogue_LeaveCamp();RogueUI_Clear();sfxForward();return true; }
        if(changed) { sfxForward();campDraw(zone); }
    }
    return false;
}

void RogueUI_IntroFrame(void)
{
    const RogueEncounter* encounter;
    char opponent[96];
    const char* player;
    int i;

    if (ready)
        return;

    openCanvas();
    ui_begin();
    ui_backdrop();

    encounter = &g_rogue_run.current_encounter;
    player = RogueRoute_CharacterName(g_rogue_run.player_kind);
    encounterOpponent(opponent, sizeof(opponent), encounter);

    ui_at(-14.2f, -12.0f, .035f, ui_muted,
          "STAGE %d", g_rogue_run.floor);
    ui_at(9.2f, -11.35f, .016f, ui_white,
          "ACT %d-%d", encounter->act, encounter->act_floor);
    ui_rule(-14.0f, -9.7f, 28.0f, ui_red);

    ui_panel_box(-14.0f, -7.8f, 11.4f, 10.5f, ui_panel);
    ui_panel_box(2.6f, -7.8f, 11.4f, 10.5f, ui_panel);

    ui_at(-13.2f, -6.9f, .0135f, ui_muted, "PLAYER");
    ui_at(3.4f, -6.9f, .0135f, ui_muted,
          encounter->enemy_count > 1 ? "OPPONENTS" : "OPPONENT");

    ui_fighter_icon(g_rogue_run.player_kind, g_rogue_run.player_costume,
                    -8.4f, -1.8f, 3.4f);

    if (encounter->enemy_count <= 1) {
        ui_fighter_icon(encounter->enemy_kind,
                        encounter->enemy_count ?
                        encounter->enemies[0].costume : 0,
                        8.4f, -1.8f, 3.4f);
    } else {
        for (i = 0; i < encounter->enemy_count && i < 3; ++i) {
            float x = 6.0f + i * 2.4f;
            ui_fighter_icon(encounter->enemies[i].kind,
                            encounter->enemies[i].costume,
                            x, -1.8f, 2.35f);
        }
    }

    ui_at(-13.2f, .55f, .023f, ui_white, "%s", player);
    ui_at(3.4f, .55f, strlen(opponent) > 18 ? .018f : .022f,
          ui_white, "%s", opponent);

    ui_at(-1.45f, -2.0f, .050f, ui_orange, "VS");

    ui_at(-13.1f, 4.25f, .018f, ui_gold, "%s",
          RogueRoute_StageName(encounter->stage));
    ui_at(-13.1f, 6.0f, .015f, ui_white, "%s",
          RogueRoute_TypeName(encounter->type));

    if (encounter->modifier)
        ui_at(-13.1f, 7.55f, .0145f, ui_orange, "%s",
              encounter->modifier);

    ui_at(8.4f, 10.2f, .014f, ui_muted, "NOW LOADING");
}
