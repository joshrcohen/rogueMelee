#include "rogue_items.h"
#include <sysdolphin/baselib/gobj.h>
#include <melee/it/types.h>
/* Native item manager permits fewer than 128 simultaneous items. Dead slots
 * are reclaimed by checking the live list; overflow fails closed. */
static struct { Item* item; u32 id; bool used; } states[128];
static u32 serial;
void RogueItem_Spawn(Item* item)
{
    int vacant=-1;
    for(int i=0;i<128;i++) {
        if(states[i].item==item) { vacant=i; break; }
        if(!states[i].item) vacant=i;
    }
    if(vacant<0) for(int i=0;i<128;i++) {
        bool live=false;
        for(HSD_GObj* g=HSD_GObjPLinkHead[HSD_GOBJ_PLINK_ITEM];g;g=g->next)
            if(g->user_data==states[i].item) { live=true;break; }
        if(!live) { vacant=i;break; }
    }
    if(vacant>=0) { states[vacant].item=item;states[vacant].used=false;states[vacant].id=++serial; }
}
bool* RogueItem_Pierced(Item* item)
{
    static bool denied=true;
    for(int i=0;i<128;i++) if(states[i].item==item) return &states[i].used;
    return &denied;
}
u32 RogueItem_ID(Item* item)
{
    for(int i=0;i<128;i++) if(states[i].item==item) return states[i].id;
    return 0;
}
