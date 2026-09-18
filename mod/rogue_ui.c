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
#include <sysdolphin/baselib/sislib.h>
#include <sysdolphin/baselib/controller.h>
#include <dolphin/gx.h>
#include <melee/lb/lbaudio_ax.h>
#include <stdarg.h>
#include <stdio.h>
#include <printf.h>
#include <string.h>
static HSD_Text* lines[128];
static unsigned line_count;
static int overlay = 1, overlay_canvas;
extern u8 mn_804D6BB4;
static GXColor ui_white = {237, 239, 250, 255};
static GXColor ui_gold = {255, 200, 0, 255};
static GXColor ui_muted = {175, 187, 215, 255};
static GXColor ui_dark = {9, 15, 35, 230};
static GXColor ui_black = {18, 21, 34, 255};

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
    HSD_Text* t = HSD_SisLib_803A6754(0, overlay ? overlay_canvas : mn_804D6BB4);
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

static int cursor, page, delay, camp_zone = -2, camp_branch;
static bool ready, inspect, history_open, hud_ready;
static int history_cursor;
static const RogueHistoryEntry* viewed_record;
static HSD_Text* markers[5];
static float camp_y;
void RogueUI_Clear(void) { ui_begin(); ready = false; }
void RogueUI_Reset(void)
{
    /* Called after scene teardown: SIS has already released its scene objects. */
    line_count = 0; ready = false; cursor = page = delay = camp_branch = 0;
    camp_zone = -2; inspect = false;
    RogueCamp_Reset();
    history_open=false;history_cursor=0;viewed_record=NULL;
    hud_ready = false;
    memset(markers, 0, sizeof(markers));
}
static void openCanvas(void)
{
    if (ready) return;
    overlay_canvas = HSD_SisLib_803A611C(0, NULL, 9, 13, 0, 20, 0, 15);
    ready = true;
}

void RogueUI_HudFrame(void)
{
    const RogueEncounter* encounter;
    const RogueStats* stats;
    HSD_Text* shadow;
    HSD_Text* panel;
    HSD_Text* accent;
    char raw[128], encoded[128];
    int fight;

    if (hud_ready || !Rogue_IsActive() ||
        g_rogue_run.phase != ROGUE_PHASE_ENCOUNTER)
        return;

    openCanvas();
    encounter = &g_rogue_run.current_encounter;
    stats = &g_rogue_run.stats;
    fight = (encounter->act - 1) * ROGUE_FLOORS_PER_ACT +
            encounter->act_floor;

    /*
     * Melee-like compact HUD:
     * - subtle drop shadow
     * - dark body
     * - gold accent tab
     * - only the information you can use at a glance during gameplay
     */
    shadow = ui_object(-18.92f, -13.86f, .0112f, ui_black);
    shadow->bg_color = ui_black;
    shadow->box_size_x = 12.55f / .0112f;
    shadow->box_size_y = 3.15f / .0112f;

    panel = ui_object(-18.78f, -13.72f, .0112f, ui_white);
    panel->bg_color = ui_dark;
    panel->box_size_x = 12.25f / .0112f;
    panel->box_size_y = 2.85f / .0112f;

    accent = ui_object(-18.78f, -13.72f, .0108f, ui_black);
    accent->bg_color = ui_gold;
    accent->box_size_x = 2.35f / .0108f;
    accent->box_size_y = 0.92f / .0108f;
    ui_encode(encoded, "ROGUE");
    HSD_SisLib_803A6B98(accent, 10.0f, 6.0f, "%s", encoded);

    snprintf(raw, sizeof(raw), "FIGHT %d/%d   %.18s",
             fight, ROGUE_RUN_ENCOUNTERS,
             encounter->name ? encounter->name : "Encounter");
    ui_at(-16.15f, -13.15f, .0105f, ui_white, "%s", raw);

    ui_at(-18.42f, -12.12f, .0093f, ui_muted,
          "A%d-%d  G%d  DMG %+.0f%%  RUN %+.0f%%  SH %+.0f%%",
          encounter->act, encounter->act_floor, g_rogue_run.currency,
          (stats->damage_dealt - 1.0f) * 100,
          (stats->run_speed - 1.0f) * 100,
          (stats->shield_health - 1.0f) * 100);

    hud_ready = true;
}

static void buildPage(void)
{
    const RogueStats* s = viewed_record ? &viewed_record->stats : &g_rogue_run.stats;
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
static void draw(void)
{
    ui_begin();
    if(inspect) { buildPage(); return; }
    if(history_open) {
        ui_at(-13,-11,.035f,ui_gold,"RUN HISTORY");
        int count=RogueHistory_Count();
        if(!count) ui_at(-13,-5,.022f,ui_white,"No completed runs yet.");
        for(int i=0;i<3;i++) {
            int index=(history_cursor/3)*3+i;
            const RogueHistoryEntry* e=RogueHistory_Get(index);if(!e) break;
            char title[100],detail[100];
            snprintf(title,sizeof(title),"%d. %s  -  %d / 15 wins",index+1,e->complete?"Victory":"Defeat",e->wins);
            snprintf(detail,sizeof(detail),"Fighter %d   Seed %u   Gold %d",e->character,e->seed,e->currency);
            ui_card(-6+i*4.2f,3.7f,index==history_cursor,title,detail,"A: inspect final build");
        }
        ui_at(-13,9,.018f,ui_white,"Control Stick: choose     A: inspect     B: return");
        if(RogueHistory_Status()<0) ui_at(-13,11,.015f,ui_gold,"Memory card unavailable. Recent history is in memory.");
        return;
    }
    bool reward=g_rogue_run.phase==ROGUE_PHASE_REWARD;
    ui_at(-13.2f,-11,.035f,ui_gold,reward?"VICTORY":g_rogue_run.phase==ROGUE_PHASE_COMPLETE?"RUN COMPLETE":"RUN OVER");
    ui_at(-13.2f,-8.4f,.019f,ui_white,"Act %d   Floor %d / 5   Gold %d",g_rogue_run.current_encounter.act,g_rogue_run.current_encounter.act_floor,g_rogue_run.currency);
    for(int i=0;i<(reward?3:4);i++) {
        char detail[256], extra[100];
        if(reward) {
            RogueReward* r=&g_rogue_run.current_rewards[i];
            Rogue_DescribeReward(r,detail,sizeof(detail));
            /* Wrap the description into two rows without splitting words. */
            int split=(int)strlen(detail);
            extra[0]=0;
            if(split>62) { split=62; while(split>0 && detail[split]!=' ') --split;
                if(split>0) { snprintf(extra,sizeof(extra),"%s",detail+split+1);detail[split]=0; } }
            char title[100];snprintf(title,sizeof(title),"%s  -  %s",r->name,Rogue_RarityName(r->rarity));
            ui_card(-5.6f+i*4.2f,3.7f,cursor==i,title,detail,extra);
        } else {
            const char* names[]={"New run","Replay seed","Run history","Main menu"};
            const char* descriptions[]={"Choose a fighter and begin a new run.","Choose a fighter using this same seed.","Inspect the last 30 runs and their builds.","Return to 1-P Mode."};
            ui_card(-5.6f+i*3.2f,2.8f,cursor==i,names[i],descriptions[i],NULL);
        }
    }
    ui_at(-13.2f,9,.018f,ui_white,"Control Stick: choose     A: confirm     B: build");
}
void RogueUI_OpenResults(void) { openCanvas(); cursor=page=delay=0;inspect=false;draw(); }
int RogueUI_Frame(void)
{
    if(++delay<30) return ROGUE_UI_WAIT;
    u32 input=mn_80229624(Rogue_ControllerPort());
    if(input&MenuInput_Back) {
        if(inspect) { inspect=false;viewed_record=NULL; }
        else if(history_open) history_open=false;
        else inspect=true;
        sfxBack();draw();return ROGUE_UI_WAIT;
    }
    if(inspect) {
        if(input&(MenuInput_Left|MenuInput_LTrigger)) { page=(page+3)%4;draw();sfxMove(); }
        if(input&(MenuInput_Right|MenuInput_RTrigger)) { page=(page+1)%4;draw();sfxMove(); }
        return ROGUE_UI_WAIT;
    }
    if(history_open) {
        int count=RogueHistory_Count();
        if(count && (input&MenuInput_Up)) { history_cursor=(history_cursor+count-1)%count;draw();sfxMove(); }
        if(count && (input&MenuInput_Down)) { history_cursor=(history_cursor+1)%count;draw();sfxMove(); }
        if(count && (input&MenuInput_Confirm)) { viewed_record=RogueHistory_Get(history_cursor);inspect=true;page=0;draw();sfxForward(); }
        return ROGUE_UI_WAIT;
    }
    int count=g_rogue_run.phase==ROGUE_PHASE_REWARD?3:4;
    if(input&MenuInput_Up) { cursor=(cursor+count-1)%count;draw();sfxMove(); }
    if(input&MenuInput_Down) { cursor=(cursor+1)%count;draw();sfxMove(); }
    if(input&MenuInput_Confirm) {
        if(g_rogue_run.phase==ROGUE_PHASE_REWARD) {
            if(!Rogue_SelectReward(cursor)) return ROGUE_UI_WAIT;
            sfxForward();return ROGUE_UI_CONTINUE;
        }
        if(cursor==2) { history_open=true;history_cursor=0;draw();sfxForward();return ROGUE_UI_WAIT; }
        sfxForward(); return cursor==0?ROGUE_UI_NEW:cursor==1?ROGUE_UI_REPLAY:ROGUE_UI_EXIT;
    }
    return ROGUE_UI_WAIT;
}
/* Camp signs are projected from world coordinates every frame. Walking and
 * jumping continue normally; only grounded contact with a zone permits A. */

static void campDraw(int zone)
{
    ui_begin();
    ui_at(-17,-13,.020f,ui_gold,"REST AREA    Gold %d",g_rogue_run.currency);
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
    if(ready) return;
    openCanvas();
    ui_at(-15,-11,.030f,ui_gold,"STAGE %d",g_rogue_run.floor);
    ui_at(-15,-8.5f,.021f,ui_white,"Act %d   %s",g_rogue_run.current_encounter.act,g_rogue_run.current_encounter.name);
    if (g_rogue_run.current_encounter.modifier)
        ui_at(-15,-6.2f,.017f,ui_gold,"%s",g_rogue_run.current_encounter.modifier);
}
