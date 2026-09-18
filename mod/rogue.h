#ifndef MELEE_ROGUE_H
#define MELEE_ROGUE_H

#include <melee/gm/types.h>

extern GameModeState gm_Mode_Rogue_States[];
void Rogue_ModeOnLoad(void);
void Rogue_ModeOnUnload(void);
bool Rogue_DeveloperBootRequested(void);
bool Rogue_PostFight(void);
int Rogue_ControllerPort(void);

#endif
