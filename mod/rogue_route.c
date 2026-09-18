#include "rogue_route.h"

#include "rogue_state.h"
#include <string.h>

static bool repeats_previous(const RogueRoute* route, int round,
                             const RogueEncounter* encounter)
{
    const RogueRouteRound* previous;
    int selected;

    if (round <= 0)
        return false;

    previous = &route->rounds[round - 1];
    selected = previous->selected;
    if (!previous->generated || selected < 0 ||
        selected >= ROGUE_ROUTE_CHOICES)
        return false;

    return previous->choices[selected].enemy_kind == encounter->enemy_kind;
}

static bool choices_differ(const RogueEncounter* a, const RogueEncounter* b)
{
    if (a->type != b->type)
        return true;
    if (a->enemy_kind != b->enemy_kind)
        return true;
    if (a->stage != b->stage)
        return true;
    if (a->name && b->name && strcmp(a->name, b->name) != 0)
        return true;
    return false;
}

static void generate_choice(RogueRoute* route, RogueRng* rng, int floor,
                            int round, int slot)
{
    RogueEncounter candidate;
    int attempt;

    for (attempt = 0; attempt < 12; ++attempt) {
        Rogue_GenerateEncounter(&candidate, rng, floor);

        if (repeats_previous(route, round, &candidate) && attempt < 6)
            continue;

        if (slot == 1 &&
            !choices_differ(&route->rounds[round].choices[0], &candidate) &&
            attempt < 11)
            continue;

        route->rounds[round].choices[slot] = candidate;
        return;
    }

    route->rounds[round].choices[slot] = candidate;
}

void RogueRoute_Reset(RogueRoute* route)
{
    int i;

    memset(route, 0, sizeof(*route));
    route->act = 0;
    route->current_round = 0;
    for (i = 0; i < ROGUE_ROUTE_ROUNDS; ++i)
        route->rounds[i].selected = -1;
}

bool RogueRoute_Prepare(RogueRoute* route, RogueRng* rng, int floor)
{
    int act;
    int act_floor;
    int round;
    int i;

    if (!route || !rng || floor < 1 || floor > ROGUE_RUN_ENCOUNTERS)
        return false;

    act = (floor - 1) / ROGUE_FLOORS_PER_ACT + 1;
    act_floor = (floor - 1) % ROGUE_FLOORS_PER_ACT + 1;

    if (act_floor >= ROGUE_FLOORS_PER_ACT)
        return false;

    if (route->act != act) {
        RogueRoute_Reset(route);
        route->act = act;
        Rogue_GenerateEncounter(&route->boss, rng,
                                act * ROGUE_FLOORS_PER_ACT);
        route->boss_generated = true;
    }

    round = act_floor - 1;
    if (round < 0 || round >= ROGUE_ROUTE_ROUNDS)
        return false;

    route->current_round = round;
    if (route->rounds[round].generated)
        return true;

    route->rounds[round].selected = -1;
    for (i = 0; i < ROGUE_ROUTE_CHOICES; ++i)
        generate_choice(route, rng, floor, round, i);

    route->rounds[round].generated = true;
    return true;
}

bool RogueRoute_Select(RogueRoute* route, int choice, RogueEncounter* encounter)
{
    RogueRouteRound* round;

    if (!route || !encounter || route->current_round < 0 ||
        route->current_round >= ROGUE_ROUTE_ROUNDS ||
        choice < 0 || choice >= ROGUE_ROUTE_CHOICES)
        return false;

    round = &route->rounds[route->current_round];
    if (!round->generated)
        return false;

    round->selected = choice;
    *encounter = round->choices[choice];
    return true;
}

bool RogueRoute_UseBoss(const RogueRoute* route, RogueEncounter* encounter)
{
    if (!route || !encounter || !route->boss_generated)
        return false;

    *encounter = route->boss;
    return true;
}

const RogueRouteRound* RogueRoute_Current(const RogueRoute* route)
{
    if (!route || route->current_round < 0 ||
        route->current_round >= ROGUE_ROUTE_ROUNDS)
        return NULL;
    return &route->rounds[route->current_round];
}

const char* RogueRoute_CharacterName(CharacterKind kind)
{
    switch (kind) {
    case CKind_Mario: return "MARIO";
    case CKind_Fox: return "FOX";
    case CKind_Captain: return "CAPT. FALCON";
    case CKind_Donkey: return "DONKEY KONG";
    case CKind_Koopa: return "BOWSER";
    case CKind_Link: return "LINK";
    case CKind_Mars: return "MARTH";
    case CKind_Pikachu: return "PIKACHU";
    case CKind_Purin: return "JIGGLYPUFF";
    case CKind_Samus: return "SAMUS";
    case CKind_Falco: return "FALCO";
    case CKind_Luigi: return "LUIGI";
    case CKind_GameWatch: return "MR. GAME & WATCH";
    case CKind_Kirby: return "KIRBY";
    case CKind_Mewtwo: return "MEWTWO";
    case CKind_Ness: return "NESS";
    case CKind_Peach: return "PEACH";
    case CKind_PopoNana: return "ICE CLIMBERS";
    case CKind_Yoshi: return "YOSHI";
    case CKind_Zelda: return "ZELDA";
    case CKind_Seak: return "SHEIK";
    case CKind_CLink: return "YOUNG LINK";
    case CKind_DrMario: return "DR. MARIO";
    case CKind_Emblem: return "ROY";
    case CKind_Pichu: return "PICHU";
    case CKind_Ganon: return "GANONDORF";
    case CKind_MasterH: return "MASTER HAND";
    case CKind_CrezyH: return "CRAZY HAND";
    default: return "FIGHTER";
    }
}

const char* RogueRoute_StageName(StKind stage)
{
    switch (stage) {
    case St_Kind_Battle: return "BATTLEFIELD";
    case St_Kind_Last: return "FINAL DESTINATION";
    case St_Kind_OldPupupu: return "DREAM LAND 64";
    case St_Kind_Story: return "YOSHI'S STORY";
    case St_Kind_PStadium: return "POKEMON STADIUM";
    case St_Kind_Izumi: return "FOUNTAIN OF DREAMS";
    case St_Kind_RCruise: return "RAINBOW CRUISE";
    case St_Kind_Kongo: return "KONGO JUNGLE";
    case St_Kind_Garden: return "JUNGLE JAPES";
    case St_Kind_Greens: return "GREEN GREENS";
    case St_Kind_Corneria: return "CORNERIA";
    case St_Kind_Zebes: return "BRINSTAR";
    case St_Kind_MuteCity: return "MUTE CITY";
    case St_Kind_Pura: return "POKE FLOATS";
    case St_Kind_OldKongo: return "KONGO JUNGLE 64";
    case St_Kind_Heal: return "REST AREA";
    default: return "STAGE";
    }
}

const char* RogueRoute_TypeName(RogueEncounterType type)
{
    switch (type) {
    case ROGUE_ENCOUNTER_ELITE: return "ELITE";
    case ROGUE_ENCOUNTER_SWARM: return "SWARM";
    case ROGUE_ENCOUNTER_TEAM: return "TAG TEAM";
    case ROGUE_ENCOUNTER_BOSS: return "BOSS";
    default: return "DUEL";
    }
}
