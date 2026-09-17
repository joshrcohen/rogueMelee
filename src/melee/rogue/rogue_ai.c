#include "rogue_format.h"
#include "rogue_ai.h"
#include "rogue_state.h"
#include "rogue_bridge.h"
#include "rogue_items.h"
#include <melee/ft/types.h>
#include <melee/it/types.h>
#include <melee/pl/player.h>
#include <melee/gm/gm_1A3F.h>
#include <melee/gm/gmvs.h>
#include <melee/gr/ground.h>
#include <melee/gr/grdatfiles.h>
#include <melee/gr/types.h>
#include <melee/mp/mplib.h>
#include <melee/mp/types.h>
#include <melee/lb/lb_00B0.h>
#include <sysdolphin/baselib/gobj.h>
#include <stdio.h>


static bool eligible(void)
{
    if (!pc_phillip_enabled() || !Rogue_IsActive() || gm_GetCurrentGameMode() != GM_ROGUE ||
        g_rogue_run.phase != ROGUE_PHASE_ENCOUNTER ||
        g_rogue_run.current_encounter.enemy_count != 1) return false;
    RogueEnemy* enemy = &g_rogue_run.current_encounter.enemies[0];
    StKind stage = g_rogue_run.current_encounter.stage;
    return !enemy->metal && enemy->model_scale == 1 &&
        (stage == St_Kind_Battle || stage == St_Kind_Last || stage == St_Kind_Izumi || stage == St_Kind_Story);
}

static bool randall_position(Vec3* position)
{
    // Map object 2 owns Randall's animated collision. Read its floor endpoints
    // rather than predicting animation timing from the match clock.
    UnkArchiveStruct* archive = grDatFiles_801C6330(2);
    CollJoint* joints = mpGetGroundCollJoint();
    if (!archive || !archive->unk4 || !joints || !stage_info.coll_data) return false;
    GrJoint* bindings = archive->unk4->unk8[2].unk20;
    int count = archive->unk4->unk8[2].unk24;
    if (!bindings) return false;
    for (int i = 0; i < count; ++i) {
        int index = bindings[i].x;
        if (index < 0 || index >= stage_info.coll_data->joint_count) continue;
        MapJoint* joint = joints[index].inner;
        if (!joint || joint->floor_count != 1) continue;
        int line = joint->floor_start;
        if (line < 0 || line >= stage_info.coll_data->line_count) continue;
        Vec3 a, b;
        mpLineGetV0Pos(line, &a);
        mpLineGetV1Pos(line, &b);
        position->x = (a.x + b.x) * 0.5f;
        position->y = (a.y + b.y) * 0.5f;
        return true;
    }
    return false;
}

static int player_json(char* text, size_t size, Fighter* fp, bool nana)
{
    int jumps = fp->co_attrs.max_jumps - fp->x1968_jumpsUsed;
    if (jumps < 0) jumps = 0;
    return snprintf(text, size,
        "{%s\"percent\":%u,\"facing\":%s,\"x\":%.5f,\"y\":%.5f,\"action\":%d,"
        "\"invulnerable\":%s,\"character\":%d,\"jumps_left\":%d,\"shield_strength\":%.4f,\"on_ground\":%s",
        nana ? "\"exists\":true," : "", (unsigned)fp->dmg.x1830_percent,
        fp->facing_dir > 0 ? "true" : "false", fp->cur_pos.x, fp->cur_pos.y, fp->motion_id,
        fp->x1988 || fp->x198C || fp->x221D_b6 ? "true" : "false", fp->kind, jumps, fp->shield_health,
        fp->ground_or_air == GA_Ground ? "true" : "false");
}

static void full_player(char* text, size_t size, Fighter* fp)
{
    int n = player_json(text, size, fp, false);
    unsigned b = fp->input.held_buttons[0];
    n += snprintf(text + n, size - n,
        ",\"controller\":{\"main_stick\":{\"x\":%.5f,\"y\":%.5f},"
        "\"c_stick\":{\"x\":%.5f,\"y\":%.5f},\"shoulder\":%.5f,"
        "\"buttons\":{\"A\":%d,\"B\":%d,\"X\":%d,\"Y\":%d,\"Z\":%d,\"L\":%d,\"R\":%d,\"D_UP\":%d}},\"nana\":",
        (fp->input.lstick[0].x+1)/2, (fp->input.lstick[0].y+1)/2,
        (fp->input.cstick[0].x+1)/2, (fp->input.cstick[0].y+1)/2, fp->input.triggers[0],
        !!(b&0x100), !!(b&0x200), !!(b&0x400), !!(b&0x800), !!(b&0x10), !!(b&0x40), !!(b&0x20), !!(b&8));
    HSD_GObj* partner = fp->kind == Ft_Kind_Popo ?
        Player_GetEntityAtIndex(fp->player_id, 1) : NULL;
    Fighter* nana = partner ? partner->user_data : NULL;
    if (nana && nana->kind == Ft_Kind_Nana && nana->is_sub_fighter) {
        n += player_json(text+n, size-n, nana, true);
        snprintf(text+n, size-n, "}}");
    } else snprintf(text+n, size-n, "null}");
}

void RogueAI_OnFrame(void)
{
    if (!eligible()) return;
    HSD_GObj* human = Player_GetEntity(0);
    HSD_GObj* cpu = Player_GetEntity(1);
    if (!human || !cpu) return;
    char p0[2048], p1[2048], game[12288];
    float left = 0, right = 0;
    Vec3 randall = { 0 };
    if (g_rogue_run.current_encounter.stage == St_Kind_Story &&
        !randall_position(&randall)) return;
    if (g_rogue_run.current_encounter.stage == St_Kind_Izumi) {
        HSD_GObj* ground = Ground_GetMapGObj(3);
        if (!ground || !ground->user_data) return;
        Ground* gp = ground->user_data;
        if (!gp->u.izumi.xD0 || !gp->u.izumi.xD4) return;
        Vec3 a, b;
        lb_8000B1CC(gp->u.izumi.xD0, NULL, &a);
        lb_8000B1CC(gp->u.izumi.xD4, NULL, &b);
        left = a.x < b.x ? a.y : b.y;
        right = a.x < b.x ? b.y : a.y;
    }
    full_player(p0, sizeof(p0), cpu->user_data); // Model p0 always controls CPU.
    full_player(p1, sizeof(p1), human->user_data);
    int n = snprintf(game, sizeof(game), "{\"p0\":%s,\"p1\":%s,\"stage\":%d,\"fod_platforms\":{\"left\":%.5f,\"right\":%.5f},\"randall\":{\"x\":%.5f,\"y\":%.5f},\"items\":[",
        p0, p1, g_rogue_run.current_encounter.stage, left, right, randall.x, randall.y);
    int count = 0;
    for (HSD_GObj* entity = HSD_GObjPLinkHead[HSD_GOBJ_PLINK_ITEM]; entity && count < 15; entity = entity->next) {
        Item* item = entity->user_data;
        if (!item) continue;
        n += snprintf(game+n, sizeof(game)-n, "%s{\"id\":%u,\"type\":%d,\"state\":%d,\"x\":%.5f,\"y\":%.5f}",
            count++ ? "," : "", RogueItem_ID(item), item->kind, item->msid, item->pos.x, item->pos.y);
    }
    snprintf(game+n, sizeof(game)-n, "]}");
    pc_phillip_send(gm_GetFrameCount(), game);
}

void RogueAI_ApplyInput(Fighter* fp)
{
    if (!fp || fp->player_id != 1 || fp->is_sub_fighter || !eligible()) return;
    PhillipInput input;
    if (!pc_phillip_input(gm_GetFrameCount(), &input)) return;
    fp->input.lstick[0].x = input.x; fp->input.lstick[0].y = input.y;
    fp->input.cstick[0].x = input.cx; fp->input.cstick[0].y = input.cy;
    fp->input.triggers[0] = input.shoulder;
    fp->input.held_buttons[0] = input.buttons;
}

void RogueAI_Reset(void) { pc_phillip_reset(); }
