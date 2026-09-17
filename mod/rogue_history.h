#ifndef MELEE_ROGUE_HISTORY_H
#define MELEE_ROGUE_HISTORY_H
#include "rogue_state.h"
#define ROGUE_HISTORY_COUNT 30
typedef struct RogueHistoryEntry {
    u32 seed; int character,wins,currency; bool complete;
    RogueStats stats; RogueAbilityID ability[4];
} RogueHistoryEntry;
void RogueHistory_Load(void);
void RogueHistory_Record(void);
int RogueHistory_Count(void);
const RogueHistoryEntry* RogueHistory_Get(int index);
int RogueHistory_Status(void);
#endif
