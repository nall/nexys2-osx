/*
 * Copyright (c) 2001 Stephen Williams (steve@icarus.com)
 * Copyright (c) 2001-2002 David Brownell (dbrownell@users.sourceforge.net)
 * Copyright (c) 2008 Roger Williams (rawqux@users.sourceforge.net)
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

#import "ezusb.h"

extern int ezusb_load_ram (IOUSBDeviceInterface** dev, NSString* hexfilePath,     
    const BOOL fx2, const BOOL stage)
{
    return 0;
}

extern int ezusb_load_eeprom (IOUSBDeviceInterface** dev, NSString* hexfilePath,     
    NSString* partType, const uint8_t config)
{
    return 0;
}

