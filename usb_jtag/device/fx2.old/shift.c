//-----------------------------------------------------------------------------
// Fast JTAG serial bit output for usbjtag
//-----------------------------------------------------------------------------
// Copyright (C) 2005,2006 Kolja Waschk, ixo.de
//-----------------------------------------------------------------------------
// This code is part of usbjtag. usbjtag is free software; you can redistribute
// it and/or modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation; either version 2 of the License,
// or (at your option) any later version. usbjtag is distributed in the hope
// that it will be useful, but WITHOUT ANY WARRANTY; without even the implied
// warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.  You should have received a
// copy of the GNU General Public License along with this program in the file
// COPYING; if not, write to the Free Software Foundation, Inc., 51 Franklin
// St, Fifth Floor, Boston, MA  02110-1301  USA
//-----------------------------------------------------------------------------

#include "c4sdcc.h"

sbit TDI = 0xA0+0;
sbit TDO = 0xA0+1;
sbit TCK = 0xA0+2;
sbit TMS = 0xA0+3;

void ShiftOut(BYTE c) _naked
{
  (void)c;

  _asm
;; 4 Instructions per bit, 48/4/4 = 3 MHz JTAG clock.
;; Code is written so that _TCK has about 50% duty cycle.
    mov  a,dpl
    ;; bit0
    rrc  a
    mov  _TDI,c
    setb _TCK
    ;; bit1
    rrc  a
    clr  _TCK
    mov  _TDI,c
    setb _TCK
    ;; bit2
    rrc  a
    clr  _TCK
    mov  _TDI,c
    setb _TCK
    ;; bit3
    rrc  a
    clr  _TCK
    mov  _TDI,c
    setb _TCK
    ;; bit4
    rrc  a
    clr  _TCK
    mov  _TDI,c
    setb _TCK
    ;; bit5
    rrc  a
    clr  _TCK
    mov  _TDI,c
    setb _TCK
    ;; bit6
    rrc  a
    clr  _TCK
    mov  _TDI,c
    setb _TCK
    ;; bit7
    rrc  a
    clr  _TCK
    mov  _TDI,c
    setb _TCK
    nop 
    clr  _TCK
    ret
  _endasm;
}

BYTE ShiftInOut(BYTE c) _naked
{
  (void)c;
  _asm
;; For ShiftInOut, the timing is a little more
;; critical because we have to read _TDO/shift/set _TDI
;; when _TCK is low. But 20% duty cycle at 48/4/5 MHz
;; is just like 50% at 6 Mhz, and thats still acceptable
    mov  a,dpl
    ;; bit0
    mov  c,_TDO
    rrc  a
    mov  _TDI,c
    setb _TCK
    clr  _TCK
    ;; bit1
    mov  c,_TDO
    rrc  a
    mov  _TDI,c
    setb _TCK
    clr  _TCK
    ;; bit2
    mov  c,_TDO
    rrc  a
    mov  _TDI,c
    setb _TCK
    clr  _TCK
    ;; bit3
    mov  c,_TDO
    rrc  a
    mov  _TDI,c
    setb _TCK
    clr  _TCK
    ;; bit4
    mov  c,_TDO
    rrc  a
    mov  _TDI,c
    setb _TCK
    clr  _TCK
    ;; bit5
    mov  c,_TDO
    rrc  a
    mov  _TDI,c
    setb _TCK
    clr  _TCK
    ;; bit6
    mov  c,_TDO
    rrc  a
    mov  _TDI,c
    setb _TCK
    clr  _TCK
    ;; bit7
    mov  c,_TDO
    rrc  a
    mov  _TDI,c
    setb _TCK
    clr  _TCK
    mov  dpl,a
    ret
  _endasm;
}

