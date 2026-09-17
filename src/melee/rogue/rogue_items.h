#ifndef MELEE_ROGUE_ITEMS_H
#define MELEE_ROGUE_ITEMS_H
#include <melee/it/forward.h>
void RogueItem_Spawn(Item* item);
bool* RogueItem_Pierced(Item* item);
u32 RogueItem_ID(Item* item);
#endif
