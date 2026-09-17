#include "rogue_bridge.h"
#include <string.h>
/* Versioned big-endian RAM mailbox for tools/dolphin_phillip.py. The host
 * publishes sequence fields last. No network or model code executes on GC. */
volatile struct {
    char magic[16]; u32 version,enabled,session,frame,length,reply_session,reply_frame;
    PhillipInput input;
    char observation[16384];
} rogue_phillip_mailbox = { "ROGUE_PHILLIP_01", 1 };
bool pc_phillip_enabled(void) { return rogue_phillip_mailbox.enabled==1; }
void pc_phillip_reset(void) { ++rogue_phillip_mailbox.session;rogue_phillip_mailbox.reply_session=0; }
void pc_phillip_send(unsigned frame,const char* observation)
{
    unsigned length=strlen(observation);
    if(length>=sizeof(rogue_phillip_mailbox.observation)) return;
    memcpy((void*)rogue_phillip_mailbox.observation,observation,length+1);
    rogue_phillip_mailbox.length=length;
    rogue_phillip_mailbox.frame=frame;
}
bool pc_phillip_input(unsigned frame,PhillipInput* input)
{
    u32 reply=rogue_phillip_mailbox.reply_frame;
    if(rogue_phillip_mailbox.reply_session!=rogue_phillip_mailbox.session || frame-reply>8) return false;
    memcpy(input,(void*)&rogue_phillip_mailbox.input,sizeof(*input));
    if(reply!=rogue_phillip_mailbox.reply_frame ||
       !(input->x>=-1 && input->x<=1 && input->y>=-1 && input->y<=1 &&
         input->cx>=-1 && input->cx<=1 && input->cy>=-1 && input->cy<=1 &&
         input->shoulder>=0 && input->shoulder<=1)) return false;
    input->buttons &= 0xF78;
    return true;
}
