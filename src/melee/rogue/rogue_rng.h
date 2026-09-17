#ifndef MELEE_ROGUE_RNG_H
#define MELEE_ROGUE_RNG_H

#include <Runtime/platform.h>

typedef struct RogueRng {
    u64 state;
} RogueRng;

void RogueRng_Seed(RogueRng* rng, u32 seed);
u32 RogueRng_Next(RogueRng* rng);
u32 RogueRng_Bounded(RogueRng* rng, u32 bound);

#endif
