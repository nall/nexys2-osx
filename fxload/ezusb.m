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

/*
 * This file contains functions for downloading firmware into Cypress
 * EZ-USB microcontrollers. These chips use control endpoint 0 and vendor
 * specific commands to support writing into the on-chip SRAM. They also
 * support writing into the CPUCS register, which is how we reset the
 * processor after loading firmware (including the reset vector).
 *
 * A second stage loader must be used when writing to off-chip memory,
 * or when downloading firmare into the bootstrap I2C EEPROM which may
 * be available in some hardware configurations.
 *
 * These Cypress devices are 8-bit 8051 based microcontrollers with
 * special support for USB I/O.  They come in several packages, and
 * some can be set up with external memory when device costs allow.
 * Note that the design was originally by AnchorChips, so you may find
 * references to that vendor (which was later merged into Cypress).
 * The Cypress FX parts are largely compatible with the Anchorhip ones.
 */

static int ram_poke (void* context, const uint16_t addr,  const BOOL external,
    const uint8_t *data, const size_t len);

static int eeprom_poke (void* context, const uint16_t addr, const BOOL external,
    const uint8_t* data, const size_t len);
    
static int parse_ihex (FILE* image, void* context,
                BOOL (*is_external)(const uint16_t addr, const size_t len),
                int (*poke) (void *context, const uint16_t addr, BOOL external,
                    const uint8_t *data, const size_t len));


static BOOL fx_is_external(const uint16_t addr, const size_t len)
{
    /* with 8KB RAM, 0x0000-0x1b3f can be written
     * we can't tell if it's a 4KB device here
     */
    if (addr <= 0x1b3f)
    {
        return ((addr + len) > 0x1b40);        
    }

    /* there may be more RAM; unclear if we can write it.
     * some bulk buffers may be unused, 0x1b3f-0x1f3f
     * firmware can set ISODISAB for 2KB at 0x2000-0x27ff
     */
    return TRUE;
}

/*
 * return true iff [addr,addr+len) includes external RAM
 * for Cypress EZ-USB FX2
 */
static BOOL fx2_is_external (const uint16_t addr, const size_t len)
{
    /* 1st 8KB for data/code, 0x0000-0x1fff */
    if (addr <= 0x1fff)
    {
        return ((addr + len) > 0x2000);        
    }

    /* and 512 for data, 0xe000-0xe1ff */
    else if(addr >= 0xe000 && addr <= 0xe1ff)    
    {
            return ((addr + len) > 0xe200);
    }

    /* otherwise, it's certainly external */
    else
    {
        return TRUE;        
    }
}

/*
 * return true iff [addr,addr+len) includes external RAM
 * for Cypress EZ-USB FX2LP
 */
static BOOL fx2lp_is_external (const uint16_t addr, const size_t len)
{
    /* 1st 16KB for data/code, 0x0000-0x3fff */
    if (addr <= 0x3fff)
    {
        return ((addr + len) > 0x4000);        
    }

    /* and 512 for data, 0xe000-0xe1ff */
    else if (addr >= 0xe000 && addr <= 0xe1ff)
    {
        return ((addr + len) > 0xe200);        
    }

    /* otherwise, it's certainly external */
    else
    {
        return TRUE;        
    }
}

static inline int ctrl_msg(IOUSBDeviceInterface** dev, const UInt8 requestType,
    const UInt8 request, const UInt16 value, const UInt16 index,
    void* data, const size_t length)
{
    if(length > USHRT_MAX)
    {
        NSLog(@"length (%d) too big (max = %d)", length, USHRT_MAX);
        return -EINVAL;
    }
    
    IOUSBDevRequest req;
    req.bmRequestType = requestType;
    req.bRequest = request;
    req.wValue = value;
    req.wIndex = index;
    req.wLength = (UInt16)length;
    req.pData = data;
    req.wLenDone = 0;
    
    IOReturn rc = (*dev)->DeviceRequest(dev, &req);
    
    if(rc != kIOReturnSuccess)
    {
        return -1;
    }
    
    return req.wLenDone;
}

/*
 * Issues the specified vendor-specific read request.
 */
static int ezusb_read (IOUSBDeviceInterface** dev, NSString* label,
    const uint8_t opcode, const uint16_t addr, const uint8_t* data,
    const size_t len)
{
    if(verbose)
    {
        NSLog(@"%@, addr 0x%04x len %4zd (0x%04zx)", label, addr, len, len);
    }
    
    const int status = ctrl_msg(dev,
        USBmakebmRequestType(kUSBIn, kUSBVendor, kUSBDevice),
        opcode, addr, 0, (uint8_t*)data, len);
    if(status != len)
    {
        NSLog(@"%@: %d", label, status);
    } 
    
    return status;
}

/*
 * Issues the specified vendor-specific write request.
 */
static int ezusb_write (IOUSBDeviceInterface** dev, NSString* label,
    const uint8_t opcode, const uint16_t addr, const uint8_t* data,
    const size_t len)
{
    if(verbose)
    {
        NSLog(@"%@, addr 0x%04x len %4zd (0x%04zx)", label, addr, len, len);
    }
    
    const int status = ctrl_msg(dev,
        USBmakebmRequestType(kUSBOut, kUSBVendor, kUSBDevice),
        opcode, addr, 0, (uint8_t*)data, len);
    if(status != len)
    {
        NSLog(@"%@: %d", label, status);
    } 
    
    return status;
}

/*
 * Modifies the CPUCS register to stop or reset the CPU.
 * Returns false on error.
 */
static BOOL ezusb_cpucs (IOUSBDeviceInterface** dev, const uint16_t addr,
    const BOOL doRun)
{
    uint8_t data = doRun ? 0 : 1;

    if(verbose)
    {
        NSLog(@"%@", data ? @"stop CPU" : @"reset CPU");
    }
    
    const int status = ctrl_msg(dev,
        USBmakebmRequestType(kUSBOut, kUSBVendor, kUSBDevice),
        RW_INTERNAL, addr, 0, &data, 1);
    if(status != 1)
    {
        NSLog(@"Can't modify CPUCS");
        return FALSE;
    }
    
    // Successful
    return TRUE;
}


/*
 * Returns the size of the EEPROM (assuming one is present).
 * *data == 0 means it uses 8 bit addresses (or there is no EEPROM),
 * *data == 1 means it uses 16 bit addresses
 */
static inline int ezusb_get_eeprom_type (IOUSBDeviceInterface** dev, uint8_t* data)
{
    return ezusb_read (dev, @"get EEPROM size", GET_EEPROM_SIZE, 0, data, 1);
}


/*
 * Load an Intel HEX file into target RAM. The fd is the open "usbfs"
 * device, and the path is the name of the source file. Open the file,
 * parse the bytes, and write them in one or two phases.
 *
 * If stage == 0, this uses the first stage loader, built into EZ-USB
 * hardware but limited to writing on-chip memory or CPUCS.  Everything
 * is written during one stage, unless there's an error such as the image
 * holding data that needs to be written to external memory.
 *
 * Otherwise, things are written in two stages.  First the external
 * memory is written, expecting a second stage loader to have already
 * been loaded.  Then file is re-parsed and on-chip memory is written.
 */
int ezusb_load_ram (IOUSBDeviceInterface** dev, NSString* hexfilePath,     
    const part_type partType, const BOOL stage)
{
    FILE* image;
    uint16_t cpucs_addr;
    BOOL (*is_external)(const uint16_t off, const size_t len);
    struct ram_poke_context ctx;
    uint32_t status;

    image = fopen ([hexfilePath cStringUsingEncoding:NSASCIIStringEncoding], "r");
    if (image == 0)
    {
        NSLog(@"%@: unable to open for input.", hexfilePath);
        return -2;
    }
    else if (verbose)
    {
        NSLog(@"open RAM hexfile image %@", hexfilePath);        
    }

    /* EZ-USB original/FX and FX2 devices differ, apart from the 8051 core */
    if (partType == ptFX2LP)
    {
        cpucs_addr = 0xe600;
        is_external = fx2lp_is_external;
    }
    else if (partType == ptFX2)
    {
        cpucs_addr = 0xe600;
        is_external = fx2_is_external;
    }
    else
    {
        cpucs_addr = 0x7f92;
        is_external = fx_is_external;
    }

    /* use only first stage loader? */
    if (stage == FALSE)
    {
        ctx.mode = internal_only;

        /* don't let CPU run while we overwrite its code/data */
        if (ezusb_cpucs (dev, cpucs_addr, 0) == FALSE)
        {
            return -1;            
        }

        /* 2nd stage, first part? loader was already downloaded */
    }
    else
    {
        ctx.mode = skip_internal;

        /* let CPU run; overwrite the 2nd stage loader later */
        if (verbose)
        {
            NSLog(@"2nd stage:  write external memory");
        }
    }

    /* scan the image, first (maybe only) time */
    ctx.dev = dev;
    ctx.total = ctx.count = 0;
    status = parse_ihex (image, &ctx, is_external, ram_poke);
    if (status < 0)
    {
        NSLog(@"unable to download %@", hexfilePath);
        return status;
    }

    /* second part of 2nd stage: rescan */
    if (stage == TRUE)
    {
        ctx.mode = skip_external;

        /* don't let CPU run while we overwrite the 1st stage loader */
        if (ezusb_cpucs (dev, cpucs_addr, 0) == FALSE)
        {
            return -1;            
        }

        /* at least write the interrupt vectors (at 0x0000) for reset! */
        rewind (image);
        if (verbose)
        {
            NSLog(@"2nd stage:  write on-chip memory");            
        }
            
        status = parse_ihex (image, &ctx, is_external, ram_poke);
        if (status < 0)
        {
            NSLog(@"unable to completely download %@", hexfilePath);
            return status;
        }
    }

    if (verbose)
    {
        NSLog(@"... WROTE: %d bytes, %d segments, avg %d",
            ctx.total, ctx.count, ctx.total / ctx.count);        
    }

    /* now reset the CPU so it runs what we just downloaded */
    if (ezusb_cpucs (dev, cpucs_addr, 1) == FALSE)
    {
        return -1;        
    }

    return 0;
}

/*
 * Load an Intel HEX file into target (large) EEPROM, set up to boot from
 * that EEPROM using the specified microcontroller-specific config byte.
 * (Defaults:  FX2 0x08, FX 0x00, AN21xx n/a)
 *
 * Caller must have pre-loaded a second stage loader that knows how
 * to handle the EEPROM write requests.
 */
int ezusb_load_eeprom (IOUSBDeviceInterface** dev, NSString* hexfilePath,     
    const part_type partType, uint8_t config)
{
    FILE* image;
    uint16_t cpucs_addr;
    BOOL (*is_external)(const uint16_t off, const size_t len);
    struct eeprom_poke_context ctx;
    uint32_t status;
    uint8_t value, first_byte;

    if (ezusb_get_eeprom_type (dev, &value) != 1 || value != 1)
    {
        NSLog(@"don't see a large enough EEPROM");
        return -1;
    }

    image = fopen ([hexfilePath cStringUsingEncoding:NSASCIIStringEncoding], "r");
    if (image == 0)
    {
        NSLog(@"%@: unable to open for input.", hexfilePath);
        return -2;
    }
    else if (verbose)
    {
        NSLog(@"open EEPROM hexfile image %@", hexfilePath);        
    }

    if (verbose)
    {
        NSLog(@"2nd stage:  write boot EEPROM");        
    }

    /* EZ-USB family devices differ, apart from the 8051 core */
    if(partType == ptFX2)
    {
        first_byte = 0xC2;
        cpucs_addr = 0xe600;
        is_external = fx2_is_external;
        ctx.ee_addr = 8;
        config &= 0x4f;
        NSLog(@
            "FX2:  config = 0x%02x, %sconnected, I2C = %d KHz",
            config,
            (config & 0x40) ? "dis" : "",
            // NOTE:  old chiprevs let CPU clock speed be set
            // or cycle inverted here.  You shouldn't use those.
            // (Silicon revs B, C?  Rev E is nice!)
            (config & 0x01) ? 400 : 100
            );
    }
    else if(partType == ptFX2LP)
    {
        first_byte = 0xC2;
        cpucs_addr = 0xe600;
        is_external = fx2lp_is_external;
        ctx.ee_addr = 8;
        config &= 0x4f;
        fprintf (stderr,
            "FX2LP:  config = 0x%02x, %sconnected, I2C = %d KHz",
            config,
            (config & 0x40) ? "dis" : "",
            (config & 0x01) ? 400 : 100
            );
    }
    else if(partType == ptFX)
    {
        first_byte = 0xB6;
        cpucs_addr = 0x7f92;
        is_external = fx_is_external;
        ctx.ee_addr = 9;
        config &= 0x07;
        NSLog(@
            "FX:  config = 0x%02x, %d MHz%s, I2C = %d KHz",
            config,
            ((config & 0x04) ? 48 : 24),
            (config & 0x02) ? " inverted" : "",
            (config & 0x01) ? 400 : 100
            );
    }
    else if(partType == ptAN21)
    {
        first_byte = 0xB2;
        cpucs_addr = 0x7f92;
        is_external = fx_is_external;
        ctx.ee_addr = 7;
        config = 0;
        NSLog(@"AN21xx:  no EEPROM config byte");
    }
    else
    {
        NSLog(@"?? Unrecognized microcontroller type %@ ??", partType);
        return -1;
    }

    /* make sure the EEPROM won't be used for booting,
     * in case of problems writing it
     */
    value = 0x00;
    status = ezusb_write (dev, @"mark EEPROM as unbootable",
        RW_EEPROM, 0, &value, sizeof value);
    if (status < 0)
    {
        return status;        
    }

    /* scan the image, write to EEPROM */
    ctx.dev = dev;
    ctx.last = 0;
    status = parse_ihex (image, &ctx, is_external, eeprom_poke);
    if (status < 0)
    {
        NSLog(@"unable to write EEPROM %@", hexfilePath);
        return status;
    }

    /* append a reset command */
    value = 0;
    ctx.last = 1;
    status = eeprom_poke (&ctx, cpucs_addr, 0, &value, sizeof value);
    if (status < 0)
    {
        NSLog(@"unable to append reset to EEPROM %@", hexfilePath);
        return status;
    }

    /* write the config byte for FX, FX2 */
    else if(partType == ptAN21)
    {
        value = config;
        status = ezusb_write (dev, @"write config byte",
            RW_EEPROM, 7, &value, sizeof value);
        if (status < 0)
        {
            return status;            
        }
    }

    /* EZ-USB FX has a reserved byte */
    else if(partType == ptFX)
    {
        value = 0;
        status = ezusb_write (dev, @"write reserved byte",
            RW_EEPROM, 8, &value, sizeof value);
        if (status < 0)
        {
            return status;            
        }
    }

    /* make the EEPROM say to boot from this EEPROM */
    status = ezusb_write (dev, @"write EEPROM type byte",
        RW_EEPROM, 0, &first_byte, sizeof first_byte);
    if (status < 0)
    {
        return status;        
    }

    /* Note:  VID/PID/version aren't written.  They should be
     * written if the EEPROM type is modified (to B4 or C0).
     */

    return 0;
}


/*
 * Parse an Intel HEX image file and invoke the poke() function on the
 * various segments to implement policies such as writing to RAM (with
 * a one or two stage loader setup, depending on the firmware) or to
 * EEPROM (two stages required).
 *
 * image    - the hex image file
 * context  - for use by poke()
 * is_external  - if non-null, used to check which segments go into
 *        external memory (writable only by software loader)
 * poke     - called with each memory segment; errors indicated
 *        by returning negative values.
 *
 * Caller is responsible for halting CPU as needed, such as when
 * overwriting a second stage loader.
 */
int parse_ihex (FILE* image,
                void* context,
                BOOL (*is_external)(const uint16_t addr, const size_t len),
                int (*poke) (void *context, const uint16_t addr, BOOL external,
                    const uint8_t *data, const size_t len)
)
{
    uint8_t data [1023];
    uint16_t data_addr = 0;
    size_t data_len = 0;
    uint32_t first_line = 1;
    BOOL external = FALSE;
    int rc;

    /* Read the input file as an IHEX file, and report the memory segments
     * as we go.  Each line holds a max of 16 bytes, but downloading is
     * faster (and EEPROM space smaller) if we merge those lines into larger
     * chunks.  Most hex files keep memory segments together, which makes
     * such merging all but free.  (But it may still be worth sorting the
     * hex files to make up for undesirable behavior from tools.)
     *
     * Note that EEPROM segments max out at 1023 bytes; the download protocol
     * allows segments of up to 64 KBytes (more than a loader could handle).
     */
    for (;;) {
        char        buf [512], *cp;
        char        tmp, type;
        size_t      len;
        unsigned    idx, off;

        cp = fgets(buf, sizeof buf, image);
        if (cp == 0) {
            NSLog(@"EOF without EOF record!");
            break;
        }

        /* EXTENSION: "# comment-till-end-of-line", for copyrights etc */
        if (buf[0] == '#')
            continue;

        if (buf[0] != ':') {
            NSLog(@"not an ihex record: %s", buf);
            return -2;
        }

        /* ignore any newline */
        cp = strchr (buf, '\n');
        if (cp)
            *cp = 0;

        if (verbose >= 3)
            NSLog(@"** LINE: %s", buf);

        /* Read the length field (up to 16 bytes) */
        tmp = buf[3];
        buf[3] = 0;
        len = strtoul(buf+1, 0, 16);
        buf[3] = tmp;

        /* Read the target offset (address up to 64KB) */
        tmp = buf[7];
        buf[7] = 0;
        off = strtoul(buf+3, 0, 16);
        buf[7] = tmp;

   /* Initialize data_addr */
    if (first_line) {
        data_addr = off;
        first_line = 0;
    }

    /* Read the record type */
    tmp = buf[9];
    buf[9] = 0;
    type = strtoul(buf+7, 0, 16);
    buf[9] = tmp;

    /* If this is an EOF record, then make it so. */
    if (type == 1) {
        if (verbose >= 2)
        NSLog(@"EOF on hexfile");
        break;
    }

    if (type != 0) {
        NSLog(@"unsupported record type: %u", type);
        return -3;
    }

    if ((len * 2) + 11 > strlen(buf)) {
        NSLog(@"record too short?");
        return -4;
    }

    // FIXME check for _physically_ contiguous not just virtually
    // e.g. on FX2 0x1f00-0x2100 includes both on-chip and external
    // memory so it's not really contiguous

    /* flush the saved data if it's not contiguous,
     * or when we've buffered as much as we can.
     */
     if (data_len != 0
             && (off != (data_addr + data_len)
             // || !merge
             || (data_len + len) > sizeof data)) {
         if (is_external)
         external = is_external (data_addr, data_len);
         rc = poke (context, data_addr, external, data, data_len);
         if (rc < 0)
         return -1;
         data_addr = off;
         data_len = 0;
     }

     /* append to saved data, flush later */
     for (idx = 0, cp = buf+9 ;  idx < len ;  idx += 1, cp += 2) {
         tmp = cp[2];
         cp[2] = 0;
         data [data_len + idx] = strtoul(cp, 0, 16);
         cp[2] = tmp;
     }
     data_len += len;
     }


     /* flush any data remaining */
     if (data_len != 0) {
     if (is_external)
         external = is_external (data_addr, data_len);
     rc = poke (context, data_addr, external, data, data_len);
     if (rc < 0)
         return -1;
     }
     return 0;
 }

static int ram_poke (void* context, const uint16_t addr,  const BOOL external,
    const uint8_t *data, const size_t len)
{
     struct ram_poke_context *ctx = context;
     int rc = 0;
     uint32_t retry = 0;

     switch (ctx->mode)
     {
         case internal_only:     /* CPU should be stopped */
         {
             if (external)
             {
                 NSLog(@"can't write %zd bytes external memory at 0x%04x", len, addr);
                 return -EINVAL;
             }
             
             break;         
         }
         case skip_internal:     /* CPU must be running */
         {
             if (!external)
             {
                 if (verbose >= 2)
                 {
                     NSLog(@"SKIP on-chip RAM, %zd bytes at 0x%04x", len, addr);
                 }
                 
                 return 0;
             }
             break;         
         }
         case skip_external:     /* CPU should be stopped */
         {
             if (external)
             {
                  if (verbose >= 2)
                  {
                      NSLog(@"SKIP external RAM, %zd bytes at 0x%04x", len, addr);
                  }

                  return 0;
              }
              break;
         }
         default:
         {
             NSLog(@"bug");
             return -EDOM;         
         }
     }

     ctx->total += len;
     ++ctx->count;

     /* Retry this till we get a real error. Control messages are not
      * NAKed (just dropped) so time out means is a real problem.
      */
     while ((rc = ezusb_write (ctx->dev,
             external ? @"write external" : @"write on-chip",
             external ? RW_MEMORY : RW_INTERNAL,
             addr, data, len)) < 0
         && retry < RETRY_LIMIT)
     {
        if (errno != ETIMEDOUT)
        {
            break;
        }

        retry += 1;
     }
     return (rc < 0) ? -errno : 0;
 }

static int eeprom_poke (void* context, const uint16_t addr, const BOOL external,
    const uint8_t* data, const size_t len)
{
    struct eeprom_poke_context* ctx = context;
    int rc = 0;
    uint8_t header [4];

    if (external)
    {
        NSLog(@"EEPROM can't init %zd bytes external memory at 0x%04x", len, addr);
        return -EINVAL;
    }

    if (len > 1023)
    {
        NSLog(@"not fragmenting %zd bytes", len);
        return -EDOM;
    }

    /* NOTE:  No retries here.  They don't seem to be needed;
    * could be added if that changes.
    */

    /* write header */
    header [0] = len >> 8;
    header [1] = len;
    header [2] = addr >> 8;
    header [3] = addr;
    if (ctx->last)
    {
        header [0] |= 0x80;        
    }

    if ((rc = ezusb_write (ctx->dev, @"write EEPROM segment header",
         RW_EEPROM,
         ctx->ee_addr, header, 4)) < 0)
    {
        return rc;                
    }

    /* write code/data */
    if ((rc = ezusb_write (ctx->dev, @"write EEPROM segment",
         RW_EEPROM,
         ctx->ee_addr + 4, data, len)) < 0)
    {
        return rc;                
    }

    /* next shouldn't overwrite it */
    ctx->ee_addr += 4 + len;

    return 0;
    }
