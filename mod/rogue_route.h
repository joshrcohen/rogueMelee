#ifndef MELEE_ROGUE_ROUTE_H
#define MELEE_ROGUE_ROUTE_H

#include "rogue_encounter.h"
#include "rogue_rng.h"
#include <Runtime/platform.h>

#define ROGUE_ROUTE_ROUNDS 4
#define ROGUE_ROUTE_CHOICES 2

typedef struct RogueRouteRound {
    RogueEncounter choices[ROGUE_ROUTE_CHOICES];
    int selected;
    bool generated;
} RogueRouteRound;

typedef struct RogueRoute {
    int act;
    int current_round;
    RogueRouteRound rounds[ROGUE_ROUTE_ROUNDS];
    RogueEncounter boss;
    bool boss_generated;
} RogueRoute;

void RogueRoute_Reset(RogueRoute* route);
bool RogueRoute_Prepare(RogueRoute* route, RogueRng* rng, int floor);
bool RogueRoute_Select(RogueRoute* route, int choice, RogueEncounter* encounter);
bool RogueRoute_UseBoss(const RogueRoute* route, RogueEncounter* encounter);
const RogueRouteRound* RogueRoute_Current(const RogueRoute* route);

const char* RogueRoute_CharacterName(CharacterKind kind);
const char* RogueRoute_StageName(StKind stage);
const char* RogueRoute_TypeName(RogueEncounterType type);

#endif
