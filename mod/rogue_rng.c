#include "rogue_rng.h"

void RogueRng_Seed(RogueRng* rng, u32 seed)
{
    rng->state = seed;
}

/* SplitMix64: separate from HSD's gameplay RNG; seed zero is valid. */
u32 RogueRng_Next(RogueRng* rng)
{
    u64 z = (rng->state += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return (u32) ((z ^ (z >> 31)) >> 32);
}

u32 RogueRng_Bounded(RogueRng* rng, u32 bound)
{
    u32 value;
    u32 threshold;
    if (bound == 0) {
        return 0;
    }
    threshold = (u32) (-bound) % bound;
    do {
        value = RogueRng_Next(rng);
    } while (value < threshold);
    return value % bound;
}
