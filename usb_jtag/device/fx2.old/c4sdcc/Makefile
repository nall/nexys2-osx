#-----------------------------------------------------------------------------
# Makefile for FX2 library code
#-----------------------------------------------------------------------------
# Copyright (C) 2007 Kolja Waschk, ixo.de
#-----------------------------------------------------------------------------
# This code is part of usbjtag. usbjtag is free software; you can redistribute
# it and/or modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation; either version 2 of the License,
# or (at your option) any later version. usbjtag is distributed in the hope
# that it will be useful, but WITHOUT ANY WARRANTY; without even the implied
# warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.  You should have received a
# copy of the GNU General Public License along with this program in the file
# COPYING; if not, write to the Free Software Foundation, Inc., 51 Franklin
# St, Fifth Floor, Boston, MA  02110-1301  USA
#-----------------------------------------------------------------------------

CC=sdcc
CFLAGS+=-mmcs51 --no-xinit-opt -I.
CPPFLAGS+=
OBJS=stall.rel delay.rel discon.rel get_strd.rel susp.rel c4sdcc.rel \
     resume.rel get_cnfg.rel get_dscr.rel get_infc.rel i2c.rel i2c_rw.rel

AR=sdcclib

%.rel : %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $*.rel

(%.rel) : %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $*.rel
	$(AR) -a $@ $*.rel
	rm $*.rel

%.rel : %.asx
	$(AS) $(ASFLAGS) $<

(%.rel) : %.asx
	$(AS) $(ASFLAGS) $<
	$(AR) -a $@ $*.rel
	rm $*.rel

c4sdcc.lib: c4sdcc.lib($(OBJS))

c4sdcc.rel:   c4sdcc.c c4sdcc.h
stall.rel:    stall.c c4sdcc.h
delay.rel:    delay.c c4sdcc.h
discon.rel:   discon.c c4sdcc.h 
get_strd.rel: get_strd.c c4sdcc.h
susp.rel:     susp.c c4sdcc.h
resume.rel:   resume.c c4sdcc.h
get_cnfg.rel: get_cnfg.c c4sdcc.h
get_dscr.rel: get_dscr.c c4sdcc.h
get_infc.rel: get_infc.c c4sdcc.h
i2c.rel:      i2c.c c4sdcc.h
i2c_rw.rel:   i2c_rw.c c4sdcc.h

startup.rel: startup.asx
vectors.rel: vectors.asx

clean:
	rm -f *.lst *.asm *.lib *.sym *.rel *.lib *.rst


