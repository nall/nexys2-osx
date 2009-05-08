#ifndef __ezusb_H
#define __ezusb_H
/*
 * Copyright (c) 2001 Stephen Williams (steve@icarus.com)
 * Copyright (c) 2002 David Brownell (dbrownell@users.sourceforge.net)
 * Copyright (c) 2009 Jon Nall (jon.nall@gmail.com)
 *
 *    This source code is free software; you can redistribute it
 *    and/or modify it in source code form under the terms of the GNU
 *    General Public License as published by the Free Software
 *    Foundation; either version 2 of the License, or (at your option)
 *    any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

#include <mach/mach.h>
#include <IOKit/usb/IOUSBLib.h>

/*
 * These are the requests (bRequest) that the bootstrap loader is expected
 * to recognize.  The codes are reserved by Cypress, and these values match
 * what EZ-USB hardware, or "Vend_Ax" firmware (2nd stage loader) uses.
 * Cypress' "a3load" is nice because it supports both FX and FX2, although
 * it doesn't have the EEPROM support (subset of "Vend_Ax").
 */
#define RW_INTERNAL 0xA0        /* hardware implements this one */
#define RW_EEPROM   0xA2
#define RW_MEMORY   0xA3
#define GET_EEPROM_SIZE 0xA5

/*
 * For writing to RAM using a first (hardware) or second (software)
 * stage loader and 0xA0 or 0xA3 vendor requests
 */
typedef enum
{
    _undef = 0,
    internal_only,      /* hardware first-stage loader */
    skip_internal,      /* first phase, second-stage loader */
    skip_external       /* second phase, second-stage loader */
} ram_mode;

struct ram_poke_context
{
    IOUSBDeviceInterface** dev;
    ram_mode mode;
    uint32_t total;
    uint32_t count;
};

/*
 * For writing to EEPROM using a 2nd stage loader
 */
struct eeprom_poke_context
{
    IOUSBDeviceInterface** dev;
    uint16_t ee_addr;    /* next free address */
    BOOL last;
};


# define RETRY_LIMIT 5

/*
 * This function loads the firmware from the given file into RAM.
 * The file is assumed to be in Intel HEX format.  If fx2 is set, uses
 * appropriate reset commands.  Stage == 0 means this is a single stage
 * load (or the first of two stages).  Otherwise it's the second of
 * two stages; the caller preloaded the second stage loader.
 *
 * The target processor is reset at the end of this download.
 */
extern int ezusb_load_ram (
    IOUSBDeviceInterface** dev, /* usb device handle */
    NSString* hexfilePath,      /* path to hexfile */
    const BOOL fx2,             /* TRUE if this is an fx2 part; else FALSE */
    const BOOL stage            /* TRUE if this is the second stage */
    );


/*
 * This function stores the firmware from the given file into EEPROM.
 * The file is assumed to be in Intel HEX format.  This uses the right
 * CPUCS address to terminate the EEPROM load with a reset command,
 * where FX parts behave differently than FX2 ones.  The configuration
 * byte is as provided here (zero for an21xx parts) and the EEPROM
 * type is set so that the microcontroller will boot from it.
 *
 * The caller must have preloaded a second stage loader that knows
 * how to respond to the EEPROM write request.
 */
extern int ezusb_load_eeprom (
    IOUSBDeviceInterface** dev, /* usb device handle */
    NSString* hexfilePath,      /* path to hexfile */
    NSString* partType,         /* fx, fx2, an21 */
    uint8_t config        /* config byte for fx/fx2; else zero */
    );


/* boolean flag, says whether to write extra messages to stderr */
extern uint8_t verbose;
#endif
