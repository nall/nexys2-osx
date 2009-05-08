#include "c4sdcc.h"

void EZUSB_Susp(void)
{
  SUSPEND  = 0x26; // Write any byte to this reg to init suspend

  _asm
    orl  PCON,#1       ; Place the processor in idle
    nop                ; Insert some meaningless instruction
    nop                ; fetches to insure that the processor
    nop                ; suspends and resumes before RET
    nop
    nop
  _endasm;
}

