#ifndef MELEE_ROGUE_UI_H
#define MELEE_ROGUE_UI_H
#include <Runtime/platform.h>
#define ROGUE_UI_ROUTE_REWARD_BASE 100
enum { ROGUE_UI_WAIT, ROGUE_UI_CONTINUE, ROGUE_UI_NEW, ROGUE_UI_REPLAY, ROGUE_UI_EXIT };
void RogueUI_Reset(void);
void RogueUI_Clear(void);
void RogueUI_OpenResults(void);
void RogueUI_HudFrame(void);
void RogueUI_IntroFrame(void);
void RogueUI_OpenRoute(void);
int RogueUI_RouteFrame(void);
int RogueUI_Frame(void);
bool RogueUI_CampFrame(void);
#endif
