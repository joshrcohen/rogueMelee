#ifndef MELEE_ROGUE_BRIDGE_H
#define MELEE_ROGUE_BRIDGE_H
#include <Runtime/platform.h>
typedef struct PhillipInput { unsigned buttons; float x,y,cx,cy,shoulder; } PhillipInput;
bool pc_phillip_enabled(void);
void pc_phillip_reset(void);
void pc_phillip_send(unsigned frame,const char* observation);
bool pc_phillip_input(unsigned frame,PhillipInput* input);
#endif
