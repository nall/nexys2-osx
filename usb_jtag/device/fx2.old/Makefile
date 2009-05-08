#-----------------------------------------------------------------------------
# Makefile for usb_jtag FX2 firmware
#-----------------------------------------------------------------------------
# Copyright 2007 Kolja Waschk, ixo.de
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

LIBDIR=c4sdcc
LIB=c4sdcc.lib

CC=sdcc
CFLAGS+=-mmcs51 --no-xinit-opt -I${LIBDIR}

AS=asx8051
ASFLAGS+=-plosgff

## 16 kB RAM
#LDFLAGS=--code-loc 0x0000 --code-size 0x2800
#LDFLAGS+=--xram-loc 0x2800 --xram-size 0x1800

# 8 kB RAM
LDFLAGS=--code-loc 0x0000 --code-size 0x1800
LDFLAGS+=--xram-loc 0x1800 --xram-size 0x0800

LDFLAGS+=-Wl '-b USBDESCSEG = 0xE100'
LDFLAGS+=-L ${LIBDIR}

%.rel : %.asx
	$(AS) $(ASFLAGS) $<

%.rel : %.c
	$(CC) -c $(CFLAGS) $(CPPFLAGS) $< -o $@

default: std.hex

std.hex: c4sdcc/vectors.rel c4sdcc/startup.rel fw.rel usbjtag.rel shift.rel dscr.rel ${LIBDIR}/${LIB}
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $+ 

.PHONY: $(LIBDIR)/$(LIB)
${LIBDIR}/${LIB}:
	make -C ${LIBDIR}

.PHONY: boot
boot: std.hex
	-test -e /dev/usb_jtag    && /sbin/fxload -D /dev/usb_jtag    -I std.hex -t fx2
	-test -e /dev/tracii_xl2  && /sbin/fxload -D /dev/tracii_xl2  -I std.hex -t fx2
	-test -e /dev/xilinx_xpcu && /sbin/fxload -D /dev/xilinx_xpcu -I std.hex -t fx2

REF=/home/kawk/work/xilinx/xtern/xusbdfwu/xusbdfwu-1025.hex

.PHONY: ref
ref: 
	-test -e /dev/usb_jtag    && /sbin/fxload -D /dev/usb_jtag    -I ${REF} -t fx2
	-test -e /dev/tracii_xl2  && /sbin/fxload -D /dev/tracii_xl2  -I ${REF} -t fx2
	-test -e /dev/xilinx_xpcu && /sbin/fxload -D /dev/xilinx_xpcu -I ${REF} -t fx2

fw.rel: fw.c $(LIBDIR)/c4sdcc.h
usbjtag.rel: usbjtag.c $(LIBDIR)/c4sdcc.h
shift.rel: shift.c $(LIBDIR)/c4sdcc.h
dscr.rel: dscr.asx

.PHONY: clean distclean

clean:
	make -C ${LIBDIR} clean
	rm -f *.lst *.asm *.lib *.sym *.rel *.mem *.map *.rst *.lnk *.hex *.rst

distclean: clean



