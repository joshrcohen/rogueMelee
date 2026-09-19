#include "rogue_camp.h"
#include "rogue_state.h"
#include <melee/it/it_3F14.h>
#include <melee/lb/lbcollision.h>
#include <melee/mp/mplib.h>
#include <melee/gm/gmvs.h>
#include <melee/gr/ground.h>
#include <sysdolphin/baselib/gobj.h>
#include <sysdolphin/baselib/gobjobject.h>
#include <sysdolphin/baselib/gobjgxlink.h>
#include <sysdolphin/baselib/jobj.h>
#include <math.h>
#include <string.h>
#define ZONES 5
#define HALF_WIDTH 9.0f
static Vec3 positions[ZONES];
static Vec3 exit_marker;
static bool valid[ZONES],disabled[ZONES],created;
static HSD_JObj* icons[ZONES];
static int selected_zone;
const Vec3* RogueCamp_Position(int zone) { return &positions[zone]; }
void RogueCamp_Reset(void)
{
    /* GObjs and their model copies belong to the scene heap. */
    memset(icons,0,sizeof(icons));memset(valid,0,sizeof(valid));
    created=false;selected_zone=-1;
}
static int rewardModel(RogueRewardType type)
{
    switch(type) {
    case ROGUE_REWARD_DEFENSE:case ROGUE_REWARD_SHIELD:case ROGUE_REWARD_ANCHOR:
    case ROGUE_REWARD_SECOND_SHELL:case ROGUE_REWARD_HEAVY_ARMOR:
    case ROGUE_REWARD_BRACE:case ROGUE_REWARD_MIRROR:return It_Kind_MetalB;
    case ROGUE_REWARD_MOVEMENT:case ROGUE_REWARD_AIR_CONTROL:case ROGUE_REWARD_JUMP_HEIGHT:
    case ROGUE_REWARD_EXTRA_JUMP:case ROGUE_REWARD_MOMENTUM:case ROGUE_REWARD_FAST_FALL:
    case ROGUE_REWARD_SLIDE:return It_Kind_RabbitC;
    case ROGUE_REWARD_PARRY_HEAL:case ROGUE_REWARD_BLOODLUST:case ROGUE_REWARD_LAST_STAND:
        return It_Kind_Heart;
    case ROGUE_REWARD_ABILITY:case ROGUE_REWARD_STATIC:return It_Kind_Star;
    default:return It_Kind_Sword;
    }
}
static void rect(float x,float y,float width,float height,GXColor* color)
{
    Vec3 a={x,y,2},b={x+width,y+height,2};
    lbColl_80009DD4(&a,&b,color);
}
static void drawZones(HSD_GObj* gobj,int pass)
{
    (void)gobj;
    if(pass!=1) return;
    /* The exit already has the native glowing portal; do not draw a second
     * selection target over it. */
    for(int i=0;i<ZONES-1;i++) if(valid[i]) {
        GXColor color=disabled[i]?(GXColor){110,110,125,160}:
            selected_zone==i?(GXColor){255,215,40,255}:(GXColor){205,225,255,210};
        float x=positions[i].x,y=positions[i].y+0.6f;
        /* Brackets show the exact horizontal activation bounds. */
        rect(x-HALF_WIDTH,y,HALF_WIDTH*2,.45f,&color);
        rect(x-HALF_WIDTH,y,.5f,3,&color);
        rect(x+HALF_WIDTH-.5f,y,.5f,3,&color);
    }
}
void RogueCamp_Create(void)
{
    if(created) return;
    created=true;
    HSD_GObj* ground=GObj_Create(14,7,0);
    if(ground) GObj_SetupGXLink(ground,drawZones,6,0);
    bool have_exit=false;
    /* Same map-object marker range used by Ground's native exit detection. */
    for(int marker=0x99;marker<0xB3;marker++) {
        if(Ground_801C2D24(marker,&exit_marker)) { have_exit=true;break; }
    }
    for(int i=0;i<ZONES;i++) {
        float x=have_exit?exit_marker.x-(4-i)*26:-78+i*26;
        Vec3 hit,normal;int line;u32 flags;
        valid[i]=mpCheckFloor(x,60,x,-120,0,&hit,&line,&flags,&normal,-1,-1,-1,NULL,NULL);
        positions[i]=(Vec3){x,valid[i]?hit.y:0,0};
        if(i==4) {
            valid[i]=have_exit;
            if(have_exit) positions[i]=exit_marker;
            continue;
        }
        if(!valid[i]) continue;
        int kind=i==0?rewardModel(g_rogue_run.shop_rewards[0].type):
            i==1?It_Kind_Sword:
            i==2?It_Kind_Heart:i==3?It_Kind_RabbitC:It_Kind_Star;
        Article* article=it_804D6D24?it_804D6D24[kind]:NULL;
        if(!article || !article->x10_modelDesc || !article->x10_modelDesc->x0_joint) continue;
        HSD_GObj* g=GObj_Create(14,7,0);
        if(!g) continue;
        HSD_JObj* j=HSD_JObjLoadJoint(article->x10_modelDesc->x0_joint);
        if(!j) continue;
        HSD_GObjObject_80390A70(g,HSD_GObj_JObjKind,j);
        GObj_SetupGXLink(g,HSD_GObj_JObjCallback,6,1);
        icons[i]=j;
        HSD_JObjClearFlagsAll(j,JOBJ_HIDDEN);
        /* Display copies have no pickup, physics, hitbox or item callbacks. */
        HSD_JObjSetScaleX(j,.45f);HSD_JObjSetScaleY(j,.45f);HSD_JObjSetScaleZ(j,.45f);
    }
}
int RogueCamp_ZoneAt(float x,float y,bool grounded)
{
    if(!grounded) return -1;
    /* Native All-Star portal dimensions are 10 by 20 world units. Keep the
     * same strict bounds, with A confirmation supplied by the camp UI. */
    if(valid[4] && x>exit_marker.x-5 && x<exit_marker.x+5 &&
       y>exit_marker.y-10 && y<exit_marker.y+10) return 4;
    for(int i=0;i<ZONES-1;i++) if(valid[i] &&
       x>=positions[i].x-HALF_WIDTH && x<=positions[i].x+HALF_WIDTH &&
       y>=positions[i].y-2 && y<=positions[i].y+3) return i;
    return -1;
}
void RogueCamp_Update(int selected,int branch)
{
    selected_zone=selected;
    float frame=gm_GetFrameCount();
    for(int i=0;i<ZONES;i++) {
        disabled[i]=i==0?(g_rogue_run.shop_sold[0] || branch==2):
            i==1?(branch==2):i<4?branch!=0:false;
        HSD_JObj* j=icons[i];if(!j) continue;
        if(disabled[i]) { HSD_JObjSetFlagsAll(j,JOBJ_HIDDEN);continue; }
        HSD_JObjClearFlagsAll(j,JOBJ_HIDDEN);
        HSD_JObjSetTranslateX(j,positions[i].x);
        HSD_JObjSetTranslateY(j,positions[i].y+15+1.5f*sinf(frame*.05f+i));
        HSD_JObjSetTranslateZ(j,0);
        HSD_JObjSetRotationY(j,frame*.025f);
    }
}
