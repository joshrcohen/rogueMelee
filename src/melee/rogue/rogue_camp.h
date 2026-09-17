#ifndef MELEE_ROGUE_CAMP_H
#define MELEE_ROGUE_CAMP_H
#include <Runtime/platform.h>
#include <dolphin/mtx.h>
void RogueCamp_Reset(void);
void RogueCamp_Create(void);
int RogueCamp_ZoneAt(float x,float y,bool grounded);
void RogueCamp_Update(int selected,int branch);
const Vec3* RogueCamp_Position(int zone);
#endif
