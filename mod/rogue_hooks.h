#ifndef MELEE_ROGUE_HOOKS_H
#define MELEE_ROGUE_HOOKS_H

#include <melee/ft/forward.h>

bool Rogue_IsRunPlayer(const Fighter* fp);
float Rogue_ModifyDamageDealt(const Fighter* fp, float value);
float Rogue_ModifyDamageReceived(const Fighter* fp, float value);
float Rogue_ModifyRunSpeed(const Fighter* fp, float value);
float Rogue_ModifyShieldHealth(const Fighter* fp, float value);
float Rogue_ModifyShieldRegen(const Fighter* fp, float value);
float Rogue_ModifyAttackDamage(const Fighter* attacker, const Fighter* victim,
                               float value, bool projectile);
float Rogue_ModifyKnockback(const Fighter* attacker, const Fighter* victim, float value);
float Rogue_ModifyShieldStun(const Fighter* fp, float frames);
float Rogue_ModifyArmor(const Fighter* fp, float armor);
float Rogue_ModifyHitstun(const Fighter* fp, float frames);
void Rogue_ApplyMovementStats(Fighter* fp);

#endif
