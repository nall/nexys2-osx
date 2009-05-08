#include "c4sdcc.h"
#include <stdio.h>

void EZUSB_Delay1us(void) _naked
{
  /* Just a 'ret' takes about 4 cycles */
  _asm
    ret
  _endasm;
}

/*
  Delay for 1 millisecond (1000 microseconds).
  10 cycles * 166.6 ns per cycle is 1.66 microseconds per loop.
  1000 microseconds / 1.66 = 602.  [assumes 24 MHz clock]
*/

void EZUSB_Delay1ms(void) _naked
{
  _asm
    mov  dptr,#(0xFFFF - 602)   ; long pulse for operating
loop:
    inc     dptr            ; 3 cycles
    mov     a,dpl           ; 2 cycles
    orl     a,dph           ; 2 cycles
    jnz     loop    ; 3 cycles
    ret
  _endasm;
}

/*
  Adjust the delay based on the CPU clock
  EZUSB_Delay1ms() assumes a 24MHz clock
*/

void EZUSB_Delay(WORD ms)
{
  if ((CPUCS & bmCLKSPD) == 0)              // 12Mhz
     ms = (ms + 1) / 2;                     // Round up before dividing so we can accept 1.
  else if ((CPUCS & bmCLKSPD) == bmCLKSPD1)   // 48Mhz
     ms = ms * 2;
  if(!ms) return;
  while(ms--) EZUSB_Delay1ms();
}
