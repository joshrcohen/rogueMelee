/* Desktop environment-variable diagnostics are not part of the console build. */
#include "rogue_debug.h"
#include "rogue_ui.h"
CharacterKind Rogue_DebugTestFighter(void) { return CKind_Mario; }
bool Rogue_DebugMatrixEnabled(void) { return false; }
bool Rogue_DebugAbilityTestEnabled(void) { return false; }
bool Rogue_DebugYoshiTestEnabled(void) { return false; }
void Rogue_DebugAbilityTestFrame(void) {}
bool Rogue_DebugIntroTestEnabled(void) { return false; }
void Rogue_DebugIntroTestFrame(void) { RogueUI_IntroFrame(); }
