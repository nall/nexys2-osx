/*
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
#include <IOKit/IOCFPlugIn.h>
#import <Foundation/Foundation.h>

uint8_t verbose = 0;

static NSString* VERSION_STRING = @"fxload-osx v1.0";

static NSString* const kArgHexFile = @"kArgHexFile";
static NSString* const kArgPartType = @"kArgPartType";
static NSString* const kArg2ndStage = @"kArg2ndStage";
static NSString* const kArgConfigByte = @"kArgConfigByte";
static NSString* const kArgVid = @"kArgVid";
static NSString* const kArgPid = @"kArgPid";

static void printUsage(char* argv0)
{
    fputs("usage: ", stderr);
    fputs(argv0, stderr);
    fputs(" [-vV] [-t type] [-D vid:pid]\n", stderr);
    fputs("\t\t[-I firmware_hexfile] ", stderr);
    fputs("[-s loader] [-c config_byte]\n", stderr);
    fputs("... device types:  one of an21, fx, fx2, fx2lp\n", stderr);
}

static int findDevice(IOUSBDeviceInterface*** dev, NSNumber* vid, NSNumber* pid)
{
    int rc = 0;
    mach_port_t masterPort = 0;
    kern_return_t err = IOMasterPort(MACH_PORT_NULL, &masterPort);
    if(err)
    {
        return -1;
    }
    
    NSMutableDictionary* matchingDict = (NSMutableDictionary*)IOServiceMatching(kIOUSBDeviceClassName);
    if(matchingDict == nil)
    {
        mach_port_deallocate(mach_task_self(), masterPort);
        return -1;
    }
    
    [matchingDict setValue:vid forKey:[NSString stringWithCString:kUSBVendorID encoding:NSASCIIStringEncoding]];
    [matchingDict setValue:pid forKey:[NSString stringWithCString:kUSBProductID encoding:NSASCIIStringEncoding]];
    
    io_iterator_t iterator = 0;
    err = IOServiceGetMatchingServices(masterPort, (CFMutableDictionaryRef)matchingDict, &iterator);
    
    // This adds a reference we must release below
    io_service_t usbDeviceRef = IOIteratorNext(iterator);
    
    if(usbDeviceRef == 0)
    {
        NSLog(@"Couldn't find USB device with %x:%x",
            [vid unsignedIntegerValue], [pid unsignedIntegerValue]);
        rc = -1;
    }
    
    // Now, get the locationID of this device. In order to do this, we need to
    // create an IOUSBDeviceInterface for our device. This will create the
    // necessary connections between our userland application and the kernel
    // object for the USB Device.
    else
    {
        SInt32 score;
        IOCFPlugInInterface** plugInInterface = 0;
        
        err = IOCreatePlugInInterfaceForService(usbDeviceRef,
                                                kIOUSBDeviceUserClientTypeID,
                                                kIOCFPlugInInterfaceID,
                                                &plugInInterface, &score);        
        if (err != kIOReturnSuccess || plugInInterface == 0)
        {
            NSLog(@"IOCreatePlugInInterfaceForService returned 0x%08x.", err);
            rc = -2;
        }
        else
        {
            // Use the plugin interface to retrieve the device interface.
            err = (*plugInInterface)->QueryInterface(plugInInterface,
                                                     CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID),
                                                     (LPVOID*)dev);

            // Now done with the plugin interface.
            (*plugInInterface)->Release(plugInInterface);

            if (err || dev == NULL)
            {
               NSLog(@"QueryInterface returned %d.", err);
               rc = -3;
            }
        }
    }
    
    IOObjectRelease(usbDeviceRef);
    IOObjectRelease(iterator);
    mach_port_deallocate(mach_task_self(), masterPort);

    return rc;
}

static NSDictionary* parseArgs(int argc, char* argv[], BOOL* result)
{
    int opt;
    NSMutableDictionary* args = [NSMutableDictionary dictionary];
    
    *result = TRUE;
    
    while((opt = getopt(argc, argv, "2vVhD:I:c:s:t:")) != EOF)
    {
        switch(opt)
        {
            case '2':
            {
                // original version of "-t fx2"
                [args setValue:@"fx2" forKey:kArgPartType];
                break;
            }
            case 'D':
            {
                NSString* vidpid = [NSString stringWithCString:optarg
                    encoding:NSASCIIStringEncoding];

                uint32_t vid, pid;
                if(sscanf([vidpid cStringUsingEncoding:NSASCIIStringEncoding],
                            "%x:%x", &vid, &pid) != 2)
                {
                    NSLog(@"Invalid Vid/Pid combination (%s). Must be VID:PID", optarg);
                    *result = FALSE;                        
                }
                else
                {
                    [args setValue:[NSNumber numberWithUnsignedInt:vid] forKey:kArgVid];                        
                    [args setValue:[NSNumber numberWithUnsignedInt:pid] forKey:kArgPid];                                                
                }
                break;
            }
            case 'I':
            {
                [args setValue:[NSString stringWithCString:optarg
                                                  encoding:NSASCIIStringEncoding]
                                                  forKey:kArgHexFile];
                break;
            }
            case 'c':
            {
                NSInteger config = strtoul(optarg, 0, 0);
                if(config < 0 || config > 0xFF)
                {
                    NSLog(@"Illegal config byte: %s", optarg);
                    *result = FALSE;
                }
                else
                {
                    [args setValue:[NSNumber numberWithUnsignedChar:config]
                        forKey:kArgConfigByte];
                }
                break;
            }
            case 's':
            {
                [args setValue:[NSString stringWithCString:optarg
                                                  encoding:NSASCIIStringEncoding]
                                                  forKey:kArg2ndStage];
                break;
            }
            case 't':
            {
                NSString* type = [NSString stringWithCString:optarg encoding:NSASCIIStringEncoding];
                if([type isEqualToString:@"an21"] || // original AnchorChips parts
                   [type isEqualToString:@"fx"]   || // updated Cypress versions
                   [type isEqualToString:@"fx2"]  || // Cypress USB 2.0 versions
                   [type isEqualToString:@"fx2lp"])  // updated FX2
                {
                    [args setValue:type forKey:kArgPartType];
                }
                else
                {
                    NSLog(@"Illegal microcontroller type: %s", optarg);
                    *result = FALSE;
                }
                break;
            }
            case 'V':
            {
                fputs([VERSION_STRING cStringUsingEncoding:NSASCIIStringEncoding], stderr);
                fputs("\n", stderr);
                return nil;
            }
            case 'v':
            {
                ++verbose;
                break;
            }
            case '?':
            case 'h':
            default:
            {
                *result = FALSE;
                return nil;
            }
        }
        
        if(*result == FALSE)
        {
            break;
        }
    }
    
    // Sanity Checking Below
    
    if([args valueForKey:kArgConfigByte] != nil)
    {
        if([args valueForKey:kArgPartType] == nil)
        {
            NSLog(@"Must specify microcontroller type to write to EEPROM");
            *result = FALSE;
        }
        
        if([args valueForKey:kArg2ndStage] == nil && [args valueForKey:kArgHexFile] == nil)
        {
            NSLog(@"Need 2nd stage loader and firmware to write to EEPROM");
            *result = FALSE;
        }
    }
    
    if([args valueForKey:kArgVid] == nil)
    {
        NSLog(@"Must specify -D <VID:PID>");
        *result = FALSE;
    }
    
    if([args valueForKey:kArgHexFile] == nil)
    {
        NSLog(@"Must specify -I <hexfile>");
        *result = FALSE;
    }
    
    // Add some default stuff
    if([args valueForKey:kArgPartType]== nil)
    {
        /* an21 compatible for most purposes */
        [args setValue:@"fx" forKey:kArgPartType];
    }
        
    return args;
}

int main (int argc, char * argv[]) {
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];

    BOOL result;
    NSDictionary* args = parseArgs(argc, argv, &result);
    
    if(result == FALSE)
    {
        printUsage(argv[0]);
    }
    else if(args != nil)
    {
        int status = 0;
        IOUSBDeviceInterface** dev = 0;
        status = findDevice(&dev,
                            [args valueForKey:kArgVid],
                            [args valueForKey:kArgPid]);
        if(status != 0)
        {
            goto cleanup;
        }
        
        NSString* partTypeStr = [args valueForKey:kArgPartType];
        part_type partType = _undef;
        if([partTypeStr isEqualToString:@"an21"])
        {
            partType = ptAN21;
        }
        else if([partTypeStr isEqualToString:@"fx"])
        {
            partType = ptFX;
        }
        else if([partTypeStr isEqualToString:@"fx2"])
        {
            partType = ptFX2;
        }
        else if([partTypeStr isEqualToString:@"fx2lp"])
        {
            partType = ptFX2LP;
        }
        else
        {
            NSLog(@"Unexpected part type string: %@", partTypeStr);
            goto cleanup;
        }
        
        if(verbose)
        {
            NSLog(@"Microcontroller type: %@", partTypeStr);
        }
        
        NSString* stage2 = [args valueForKey:kArg2ndStage];
        if(stage2 != nil)
        {
            // first stage: put laoder into internal memory
            if(verbose)
            {
                NSLog(@"1st stage: Load 2nd stage loader");
            }
            status = ezusb_load_ram(dev, stage2, partType, FALSE);
            if(status != 0)
            {
                goto cleanup;
            }
            
            // second stage ... write either EEPROM or RAM
            if([args valueForKey:kArgConfigByte] != nil)
            {
                const uint8_t config =
                    [[args valueForKey:kArgConfigByte] unsignedCharValue];
                status = ezusb_load_eeprom(dev,
                                           [args valueForKey:kArgHexFile],
                                           partType,
                                           config);
            }
            else
            {
                status = ezusb_load_ram(dev,
                                           [args valueForKey:kArgHexFile],
                                           partType, TRUE);
            }
        }
        else
        {
            // single stage, put into internal memory
            if(verbose)
            {
                NSLog(@"Single stage: load on-chip memory");
            }
            
            status = ezusb_load_ram(dev, [args valueForKey:kArgHexFile], partType, FALSE);
        }
        
        if(status != 0)
        {
            goto cleanup;
        }
    }

cleanup:
    
    [pool drain];
    return 0;
}
