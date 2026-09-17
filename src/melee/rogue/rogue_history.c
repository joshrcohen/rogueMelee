#include "rogue_history.h"
#include <dolphin/card.h>
#include <dolphin/os.h>
#include <melee/lb/lbcardnew.h>
#include <stddef.h>
#include <string.h>
#define SAVE_BYTES 8192
#define MAGIC 0x52474831U
#define VERSION 1
/* No process pointers or archive addresses are serialized. */
typedef struct HistorySave {
    u32 magic,version,sequence,checksum,count;
    RogueHistoryEntry entries[ROGUE_HISTORY_COUNT];
} HistorySave;
typedef char SaveFits[(sizeof(HistorySave)<=SAVE_BYTES)?1:-1];
static union { HistorySave data; u8 bytes[SAVE_BYTES]; } slots[2] ATTRIBUTE_ALIGN(32);
static u8 work[CARD_WORKAREA_SIZE] ATTRIBUTE_ALIGN(32);
static HistorySave history;
static int active_slot=-1,status;
static bool loaded,protect_unknown;
static char* names[2]={"RogueRuns_A","RogueRuns_B"};
static u32 checksum(const u8* bytes)
{
    u32 hash=2166136261U;
    for(unsigned i=0;i<SAVE_BYTES;i++) {
        u8 value=i>=12 && i<16?0:bytes[i];
        hash=(hash^value)*16777619U;
    }
    return hash;
}
static int mount(void) { lbCardNew_CompleteAllTasks(11);return CARDMount(0,work,NULL); }
void RogueHistory_Load(void)
{
    if(loaded) return;
    memset(&history,0,sizeof(history));protect_unknown=true;
    status=mount();if(status<0) return;
    bool valid[2]={false,false};int existing=0;
    for(int i=0;i<2;i++) {
        CARDFileInfo file;CARDStat stat;
        status=CARDOpen(0,names[i],&file);
        if(status==CARD_RESULT_NOFILE) continue;
        if(status<0) { CARDUnmount(0);return; }
        ++existing;status=CARDGetStatus(0,file.fileNo,&stat);
        if(status>=0 && stat.length==SAVE_BYTES) status=CARDRead(&file,slots[i].bytes,SAVE_BYTES,0);
        else status=CARD_RESULT_BROKEN;
        CARDClose(&file);
        HistorySave* h=&slots[i].data;
        valid[i]=status>=0 && h->magic==MAGIC && h->version==VERSION &&
            h->count<=ROGUE_HISTORY_COUNT && h->checksum==checksum(slots[i].bytes);
    }
    CARDUnmount(0);
    active_slot=valid[0]?0:-1;
    if(valid[1] && (!valid[0] || (s32)(slots[1].data.sequence-slots[0].data.sequence)>0)) active_slot=1;
    if(active_slot>=0) history=slots[active_slot].data;
    protect_unknown=existing && active_slot<0;
    loaded=true;status=protect_unknown?CARD_RESULT_BROKEN:0;
}
static void save(void)
{
    if(protect_unknown) { status=CARD_RESULT_BROKEN;return; }
    int next=active_slot==0?1:0;CARDFileInfo file;CARDStat stat;
    memset(slots[next].bytes,0,SAVE_BYTES);
    slots[next].data=history;
    slots[next].data.magic=MAGIC;slots[next].data.version=VERSION;
    slots[next].data.sequence=history.sequence+1;
    slots[next].data.checksum=checksum(slots[next].bytes);
    status=mount();if(status<0) return;
    status=CARDOpen(0,names[next],&file);
    if(status==CARD_RESULT_NOFILE) status=CARDCreate(0,names[next],SAVE_BYTES,&file);
    if(status>=0) {
        status=CARDGetStatus(0,file.fileNo,&stat);
        if(status>=0 && stat.length!=SAVE_BYTES) status=CARD_RESULT_BROKEN;
        if(status>=0) status=CARDWrite(&file,slots[next].bytes,SAVE_BYTES,0);
        CARDClose(&file);
    }
    CARDUnmount(0);
    if(status>=0) { active_slot=next;history.sequence=slots[next].data.sequence; }
}
void RogueHistory_Record(void)
{
    if(g_rogue_run.phase!=ROGUE_PHASE_DEAD && g_rogue_run.phase!=ROGUE_PHASE_COMPLETE) return;
    int count=history.count<ROGUE_HISTORY_COUNT?history.count:ROGUE_HISTORY_COUNT-1;
    memmove(&history.entries[1],&history.entries[0],count*sizeof(history.entries[0]));
    RogueHistoryEntry* e=&history.entries[0];memset(e,0,sizeof(*e));
    e->seed=g_rogue_run.seed;e->character=g_rogue_run.player_kind;
    e->wins=g_rogue_run.wins;e->currency=g_rogue_run.currency;
    e->complete=g_rogue_run.phase==ROGUE_PHASE_COMPLETE;e->stats=g_rogue_run.stats;
    memcpy(e->ability,g_rogue_run.ability,sizeof(e->ability));
    history.count=count+1;save();
}
int RogueHistory_Count(void) { return history.count; }
const RogueHistoryEntry* RogueHistory_Get(int index) { return index>=0 && index<(int)history.count?&history.entries[index]:NULL; }
int RogueHistory_Status(void) { return status; }
