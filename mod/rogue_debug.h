#ifndef MELEE_ROGUE_DEBUG_H
#define MELEE_ROGUE_DEBUG_H

#include <Runtime/platform.h>
#include <melee/ft/forward.h>

CharacterKind Rogue_DebugTestFighter(void);
bool Rogue_DebugMatrixEnabled(void);
void Rogue_DebugMatrixModeLoad(void);
bool Rogue_DebugMatrixFrame(void);
bool Rogue_DebugMatrixConsumeReload(void);
bool Rogue_DebugAbilityTestEnabled(void);
bool Rogue_DebugYoshiTestEnabled(void);
void Rogue_DebugAbilityTestFrame(void);
bool Rogue_DebugIntroTestEnabled(void);
void Rogue_DebugIntroTestFrame(void);

#endif
