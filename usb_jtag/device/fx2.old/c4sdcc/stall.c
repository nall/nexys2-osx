#include "c4sdcc.h"

/*
 Description:
     routine to set or clear the stall bit for the selected endpoint
 Arguments:
     epid - the USB endpoint number (direction + ep number)
     stall - if 1 set the stall bit, else clear the stall bit
*/

void modify_endpoint_stall(BYTE epid, BYTE stall) _naked
{
  (void)epid;
  (void)stall;

  _asm

  mov   a, dpl ; endpoint id
  mov   R5, modify_endpoint_stall_PARM2 ;

  mov   R6,#0  ; register index (R6)
  xrl   a,#0x01
  jz    swdone

  inc   R6
  xrl   a,#0x80      ;0x80 = 0x81 ^ 0x01.
  jz    swdone

  mov   a,R7
  anl   a,#0x0F
  rr    a
  inc   a
  mov   R6,a

swdone:

  mov   dptr,#EP1OUTCS
  mov   a,R6
  add   a,dpl
  mov   dpl,a

  movx  a,@dptr

  cjne  R5,#1,clearstall
  orl   a,#bmEPSTALL
  sjmp  done
   
clearstall:

  anl   a,(0xFF-bmEPSTALL)
   
done:

  movx  @dptr,a
   
  ret
  _endasm;
}

