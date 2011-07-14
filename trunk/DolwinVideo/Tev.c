// task of this module is approximate every TEV stage 
// to one of following modes : DECAL, MODULATE, BLEND, REPLACE or PASSCLR
// if TEV stage doesnt fits to any of supported modes, use PASSCLR
#include "GX.h"

TEVStage    tevs[16];

// in : SU TEV registers.
// out: TEV control block
// called on any changes of TEV regs
void TEVApprox()
{
}

void TEVSelectOutput(int stage)
{
}
