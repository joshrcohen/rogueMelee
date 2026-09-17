#ifndef MELEE_ROGUE_EFFECTS_H
#define MELEE_ROGUE_EFFECTS_H
#include <melee/ft/forward.h>
void RogueEffects_BeginEncounter(void);
void RogueEffects_OnFrame(void);
bool RogueEffects_OnHit(Fighter* attacker, Fighter* victim, float damage);
bool RogueEffects_TryLastStand(Fighter* fp);
void RogueEffects_OnKO(Fighter* killer, Fighter* victim);
void RogueEffects_OnDeath(Fighter* fp);
void RogueEffects_OnParry(Fighter* fp);
bool RogueEffects_CanMirror(Fighter* fp);
bool RogueEffects_TryPierce(Fighter* owner, int kind, bool* used);
bool RogueEffects_PreventShieldBreak(Fighter* fp, float capacity);
#endif
