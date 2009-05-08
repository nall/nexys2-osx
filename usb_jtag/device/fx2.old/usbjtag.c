//-----------------------------------------------------------------------------
// Code that turns a Cypress FX2 USB Controller into an USB JTAG adapter
//-----------------------------------------------------------------------------
// Copyright (C) 2005,2006 Kolja Waschk, ixo.de
//-----------------------------------------------------------------------------
// For hardware configuration, please take a look at the I/O definitions just
// below and in the TD_Init() code before you actually connect your FX2 to
// something. There are some board-specific initializations. Then compile with
// the Keil compiler & load the resulting binary into Cypress FX2 using the
// EZUSB Control Panel! Please read the README text file that should have
// come together with this source code.
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
// This file has been derived from bulksrc.c and vend_Ax.c, both taken
// from the /USB/Examples/Fx2/bulksrc/bulksrc.c delivered with CY3681
// development kit from Cypress Semiconductor. They both are
// Copyright (c) 2000 Cypress Semiconductor. All rights reserved.
//-----------------------------------------------------------------------------
#ifdef SDCC
#include "c4sdcc.h"
#else
#pragma NOIV               // Do not generate interrupt vectors
#define INTERRUPT_0  interrupt 0
#define INTERRUPT(x) interrupt x
#include "fx2.h"
#include "fx2regs.h"
#include "fx2sdly.h"            // SYNCDELAY macro
#endif

//-----------------------------------------------------------------------------
// Debug output on TXD1? Tested only with SDCC.

#undef DEBUG
//#define DEBUG 1

#ifdef DEBUG
#include <stdio.h>
#endif

// These are in shift.a51
extern void ShiftOut(BYTE c);
extern BYTE ShiftInOut(BYTE c);

//-----------------------------------------------------------------------------
// This, when defined, enables experimental code instead of the JTAG emulation.
// The FX2 slave FIFO is configured to behave like a FT245 or FT2232 in FIFO
// mode. FLAGA=nRXF, FLAGB=nTXE, SLOE=SLRD=nRD, SLWR=WR. That alone is good to
// allow an external FIFO master to /read/ from the slave FIFO with FIFOADDR=00
// (EP2). If you want to /write/, FIFOADDR has to be driven to 10 (EP6) for the
// duration of the write cycle, i.e. be active before and after SLWR=WR. That's
// a signal not provided by masters that have been designed to interface with a
// FT245.

// #define FTDEMU 1

//-----------------------------------------------------------------------------
// If you're using other pins for JTAG, please also change shift.a51!
// And make sure the I/O initialization in TD_Init is correct for your setup.
sbit TDI = 0xA0+0;
sbit TDO = 0xA0+1;
sbit TCK = 0xA0+2;
sbit TMS = 0xA0+3;
#define bmTDIOE bmBIT0
#define bmTDOOE bmBIT1
#define bmTCKOE bmBIT2
#define bmTMSOE bmBIT3

//-----------------------------------------------------------------------------
// Define USE_MOD256_OUTBUFFER:
// Saves about 256 bytes in code size, improves speed a little.
// A further optimization could be not to use an extra output buffer at 
// all, but to write directly into EP1INBUF. Not implemented yet. When 
// downloading large amounts of data _to_ the target, there is no output
// and thus the output buffer isn't used at all and doesn't slow down things.

#define USE_MOD256_OUTBUFFER 1

//-----------------------------------------------------------------------------
// Global data

static BOOL Running;

#ifdef FTDEMU
static WORD EP6_Offset;
#else
static BOOL WriteOnly;
static BYTE ClockBytes;
static WORD Pending;
#ifdef USE_MOD256_OUTBUFFER
  static BYTE FirstDataInOutBuffer;
  static BYTE FirstFreeInOutBuffer;
#else
  static WORD FirstDataInOutBuffer;
  static WORD FirstFreeInOutBuffer;
#endif

#ifdef USE_MOD256_OUTBUFFER
  /* Size of output buffer must be exactly 256 */
  #define OUTBUFFER_LEN 0x100
  /* Output buffer must begin at some address with lower 8 bits all zero */
#else
  #define OUTBUFFER_LEN 0x200
#endif
#ifdef SDCC
xdata at 0xE000 BYTE OutBuffer[OUTBUFFER_LEN];
#else
static BYTE xdata OutBuffer[OUTBUFFER_LEN] _at_ 0xE000;
#endif
#endif

extern volatile BOOL GotSUD;             // Received setup data flag
extern volatile BOOL Sleep;
extern volatile BOOL Rwuen;
extern volatile BOOL Selfpwr;

BYTE Configuration;             // Current configuration
BYTE AlternateSetting;          // Alternate settings

//-----------------------------------------------------------------------------
// "EEPROM" Content
// If you want to simulate the EEPROM attached to the FT245BM in the
// device as Altera builds it, put something meaningful in here
//-----------------------------------------------------------------------------

#include "eeprom.c"

//-----------------------------------------------------------------------------
// Task Dispatcher hooks
//   The following hooks are called by the task dispatcher.
//-----------------------------------------------------------------------------

void VC_Reset_FPGA(void)
{
   IOE &= ~(1<<4);
   EZUSB_Delay(50);
   IOE |= (1<<4);
}

void VC_Reset_IN(void)
{
   FIFORESET  = 0x80; SYNCDELAY;   // From now on, NAK all
   FIFORESET  = 0x06; SYNCDELAY;
   FIFORESET  = 0x00; SYNCDELAY;   // Restore normal behaviour

   INPKTEND   = 0x86; SYNCDELAY;
   INPKTEND   = 0x86; SYNCDELAY;

#ifdef FTDEMU
   EP6_Offset = 0;
#else
   Pending = 0;
   FirstDataInOutBuffer = 0;
   FirstFreeInOutBuffer = 0;
#endif
}

void VC_Reset_OUT(void)
{
   EP2FIFOCFG = 0x00; SYNCDELAY;   // disable AUTOOUT for EP2

   FIFORESET  = 0x80; SYNCDELAY;   // From now on, NAK all
   FIFORESET  = 0x02; SYNCDELAY;
   FIFORESET  = 0x00; SYNCDELAY;   // Restore normal behaviour

   OUTPKTEND  = 0x82; SYNCDELAY;   // arm twice (double buffered)
   OUTPKTEND  = 0x82; SYNCDELAY;

#ifdef FTDEMU
   EP2FIFOCFG = 0x10; SYNCDELAY;  // now enable AUTOOUT for EP2
#else
   ClockBytes = 0;
   WriteOnly = TRUE;
#endif
}

void TD_Init(void)              // Called once at startup
{
   WORD tmp;

   Running = FALSE;

   /* The following code depends on your actual circuit design.
      Make required changes _before_ you try the code! */

   // set the CPU clock to 48MHz, enable clock output to FPGA
   CPUCS = bmCLKOE | bmCLKSPD1;

#ifndef FTDEMU
   // Use internal 48 MHz, enable output, use "Port" mode for all pins
   IFCONFIG = bmIFCLKSRC | bm3048MHZ | bmIFCLKOE;
#else
   // Use internal 48 MHz, enable output, asynchronous slave FIFO mode
   IFCONFIG = bmIFCLKSRC | bm3048MHZ | bmIFCLKOE 
            | bmASYNC | bmIFCFG1 | bmIFCFG0;            

   PINFLAGSAB = 0;
   FIFOPINPOLAR = 0;

   /* SLRD, SLOE == RD# Input: Enables the current FIFO data byte on 
      D0...D7 when low. Fetched the next FIFO data byte (if available)
      from the receive FIFO buffer when RD# goes from high to low. */

   FIFOPINPOLAR &= ~(1<<5); // PKTEND: active low
   FIFOPINPOLAR &= ~(1<<4); // SLOE: active low
   FIFOPINPOLAR &= ~(1<<3); // SLRD: active low

   /* FLAGA == RXF# Output: When high, do not read data from the FIFO. When
      low, there is data available in the FIFO which can be read by strobing
      RD# low, then high again. */

   PINFLAGSAB |= 0x08; // FLAGA is EP2 EMPTY FLAG
   FIFOPINPOLAR |= (1<<1); // active high

   /* SLWR == WR Input: Writes the data byte on the D0...D7 pins into the transmit
      FIFO buffer when WR goes from high to low. */

   FIFOPINPOLAR |= (1<<2); // active high

   /* FLAGB == TXE# Output: When high, do not write data into the FIFO. When 
      low, data can be written into the FIFO by strobing WR high, then low. */

   PINFLAGSAB |= 0xE0; // FLAGB is EP6 FULL FLAG
   FIFOPINPOLAR |= (1<<0); // active high
#endif

#ifdef DEBUG
   UART230 |= 2;
   SCON1 = 0x52;
   SMOD1 = 0;
   printf("Debug active\n");
#endif

   // power on the FPGA and all other VCCs, assert RESETN
   IOE = 0x1F & ~(1<<4);
   OEE = 0x1F;
   EZUSB_Delay(500); // wait for supply to come up

   // The remainder of the code however should be left unchanged...

   // Make Timer2 reload at 100 Hz to trigger Keepalive packets

   tmp = 65536 - ( 48000000 / 12 / 100 );
   RCAP2H = tmp >> 8;
   RCAP2L = tmp & 0xFF;
   CKCON = 0; // Default Clock
   T2CON = 0x04; // Auto-reload mode using internal clock, no baud clock.

   // Enable Autopointer

   EXTACC = 1;  // Enable
   APTR1FZ = 1; // Don't freeze
   APTR2FZ = 1; // Don't freeze

   // define endpoint configuration

   REVCTL = 3; SYNCDELAY;          // Allow FW access to FIFO buffer

   EP1OUTCFG  = 0xA0; SYNCDELAY;
   EP1INCFG   = 0xA0; SYNCDELAY;
   EP2CFG     = 0xA2; SYNCDELAY;
   EP4CFG     = 0xA0; SYNCDELAY;
   EP6CFG     = 0xE2; SYNCDELAY;
   EP8CFG     = 0xE0; SYNCDELAY;

   EP4FIFOCFG = 0x00; SYNCDELAY;
   EP8FIFOCFG = 0x00; SYNCDELAY;

   FIFORESET  = 0x80; SYNCDELAY;   // From now on, NAK all
   FIFORESET  = 0x04; SYNCDELAY;
   FIFORESET  = 0x08; SYNCDELAY;
   FIFORESET  = 0x00; SYNCDELAY;   // Restore normal behaviour

   OUTPKTEND  = 0x84; SYNCDELAY;
   OUTPKTEND  = 0x84; SYNCDELAY;

   VC_Reset_IN();
   VC_Reset_OUT();

   VC_Reset_FPGA();
}

#ifdef FTDEMU

//-----------------------------------------------------------------------------
// TD_Poll does most of the work. EP2 OUT transfers (to FIFO master) are 
// automatic, but data from FIFO master to the host has to be read "manually"
// from EP6. There are two reasons for this; first, a status word has to be
// prepended, and second, the driver expects data on EP1 and not on EP6.
//-----------------------------------------------------------------------------

void TD_Poll(void) // Called repeatedly while the device is idle
{
   if(!Running) return;

   if(!(EP1INCS & bmEPBUSY)) // EP1 available for IN transfer?
   {
      if(!(EP68FIFOFLGS & (1<<1))) // EP6 not empty?
       {
         WORD m;
         BYTE o, n;

         AUTOPTRH2 = MSB( EP1INBUF );
         AUTOPTRL2 = LSB( EP1INBUF );

         AUTOPTR1H = MSB( EP6FIFOBUF + EP6_Offset );
         AUTOPTR1L = LSB( EP6FIFOBUF + EP6_Offset );

         XAUTODAT2 = 0x31;
         XAUTODAT2 = 0x60;
 
         m = ((EP6FIFOBCH<<8)|EP6FIFOBCL) - EP6_Offset;

         if(m <= 62)
         {
           o = m;
           if(EP68FIFOFLGS & (1<<2)) // last packet from full fifo?
             EP6_Offset = 0;
           else
             EP6_Offset += o;
         }
         else
         {
           o = 62;
           EP6_Offset += o;
         };

#ifdef DEBUG
         printf("xfer %d bytes from %u\n", n, EP6_Offset);
#endif

         for(n = 0; n < o; n++)
         {
#ifdef DEBUG
            BYTE x = XAUTODAT1;
            printf("- %02X\n", x);
            XAUTODAT2 = x;
#else
            XAUTODAT2 = XAUTODAT1;
#endif
         };

         if(EP6_Offset == 0)
         {
           INPKTEND = 0x86; SYNCDELAY;
         }

         SYNCDELAY;
         EP1INBC = 2 + o;
         SYNCDELAY;

         TF2 = 1; // Make sure there will be a short transfer soon
      }
      else if(TF2)
      {
         EP1INBUF[0] = 0x31;
         EP1INBUF[1] = 0x60;
         SYNCDELAY;
         EP1INBC = 2;
         TF2 = 0;
      };
   };
}

#else

void OutputByte(BYTE d)
{
#ifdef USE_MOD256_OUTBUFFER
   OutBuffer[FirstFreeInOutBuffer] = d;
   FirstFreeInOutBuffer = ( FirstFreeInOutBuffer + 1 ) & 0xFF;
#else
   OutBuffer[FirstFreeInOutBuffer++] = d;
   if(FirstFreeInOutBuffer >= OUTBUFFER_LEN) FirstFreeInOutBuffer = 0;
#endif
   Pending++;
}

//-----------------------------------------------------------------------------
// TD_Poll does most of the work. It now happens to behave just like the 
// combination of FT245BM and Altera-programmed EPM7064 CPLD in Altera's
// USB-Blaster. The CPLD knows two major modes: Bit banging mode and Byte
// shift mode. It starts in Bit banging mode. While bytes are received
// from the host on EP2OUT, each byte B of them is processed as follows:
//
// Please note: nCE, nCS, LED pins and DATAOUT actually aren't supported here.
// Support for these would be required for AS/PS mode and isn't too complicated,
// but I haven't had the time yet.
//
// Bit banging mode:
// 
//   1. Remember bit 6 (0x40) in B as the "Read bit".
//
//   2. If bit 7 (0x40) is set, switch to Byte shift mode for the coming
//      X bytes ( X := B & 0x3F ), and don't do anything else now.
//
//    3. Otherwise, set the JTAG signals as follows:
//        TCK/DCLK high if bit 0 was set (0x01), otherwise low
//        TMS/nCONFIG high if bit 1 was set (0x02), otherwise low
//        nCE high if bit 2 was set (0x04), otherwise low
//        nCS high if bit 3 was set (0x08), otherwise low
//        TDI/ASDI/DATA0 high if bit 4 was set (0x10), otherwise low
//        Output Enable/LED active if bit 5 was set (0x20), otherwise low
//
//    4. If "Read bit" (0x40) was set, record the state of TDO(CONF_DONE) and
//        DATAOUT(nSTATUS) pins and put it as a byte ((DATAOUT<<1)|TDO) in the
//        output FIFO _to_ the host (the code here reads TDO only and assumes
//        DATAOUT=1)
//
// Byte shift mode:
//
//   1. Load shift register with byte from host
//
//   2. Do 8 times (i.e. for each bit of the byte; implemented in shift.a51)
//      2a) if nCS=1, set carry bit from TDO, else set carry bit from DATAOUT
//      2b) Rotate shift register through carry bit
//      2c) TDI := Carry bit
//      2d) Raise TCK, then lower TCK.
//
//   3. If "Read bit" was set when switching into byte shift mode,
//      record the shift register content and put it into the FIFO
//      _to_ the host.
//
// Some more (minor) things to consider to emulate the FT245BM:
//
//   a) The FT245BM seems to transmit just packets of no more than 64 bytes
//      (which perfectly matches the USB spec). Each packet starts with
//      two non-data bytes (I use 0x31,0x60 here). A USB sniffer on Windows
//      might show a number of packets to you as if it was a large transfer
//      because of the way that Windows understands it: it _is_ a large
//      transfer until terminated with an USB packet smaller than 64 byte.
//
//   b) The Windows driver expects to get some data packets (with at least
//      the two leading bytes 0x31,0x60) immediately after "resetting" the
//      FT chip and then in regular intervals. Otherwise a blue screen may
//      appear... In the code below, I make sure that every 10ms there is
//      some packet.
//
//   c) Vendor specific commands to configure the FT245 are mostly ignored
//      in my code. Only those for reading the EEPROM are processed. See
//      DR_GetStatus and DR_VendorCmd below for my implementation.
//
//   All other TD_ and DR_ functions remain as provided with CY3681.
//
//-----------------------------------------------------------------------------
//
 
void TD_Poll(void)              // Called repeatedly while the device is idle
{
   if(!Running) return;
   
   if(!(EP1INCS & bmEPBUSY))
   {
      if(Pending > 0)
      {
         BYTE o, n;

         AUTOPTRH2 = MSB( EP1INBUF );
         AUTOPTRL2 = LSB( EP1INBUF );
       
         XAUTODAT2 = 0x31;
         XAUTODAT2 = 0x60;
       
         if(Pending > 0x3E) { n = 0x3E; Pending -= n; } 
                     else { n = Pending; Pending = 0; };
       
         o = n;

#ifdef USE_MOD256_OUTBUFFER
         AUTOPTR1H = MSB( OutBuffer );
         AUTOPTR1L = FirstDataInOutBuffer;
         while(n--)
         {
            XAUTODAT2 = XAUTODAT1;
            AUTOPTR1H = MSB( OutBuffer ); // Stay within 256-Byte-Buffer
         };
         FirstDataInOutBuffer = AUTOPTR1L;
#else
         AUTOPTR1H = MSB( &(OutBuffer[FirstDataInOutBuffer]) );
         AUTOPTR1L = LSB( &(OutBuffer[FirstDataInOutBuffer]) );
         while(n--)
         {
            XAUTODAT2 = XAUTODAT1;

            if(++FirstDataInOutBuffer >= OUTBUFFER_LEN)
            {
               FirstDataInOutBuffer = 0;
               AUTOPTR1H = MSB( OutBuffer );
               AUTOPTR1L = LSB( OutBuffer );
            };
         };
#endif
         SYNCDELAY;
         EP1INBC = 2 + o;
         TF2 = 1; // Make sure there will be a short transfer soon
      }
      else if(TF2)
      {
         EP1INBUF[0] = 0x31;
         EP1INBUF[1] = 0x60;
         SYNCDELAY;
         EP1INBC = 2;
         TF2 = 0;
      };
   };

   if(!(EP2468STAT & bmEP2EMPTY) && (Pending < OUTBUFFER_LEN-0x3F))
   {
      BYTE i, n = EP2BCL;

      AUTOPTR1H = MSB( EP2FIFOBUF );
      AUTOPTR1L = LSB( EP2FIFOBUF );

      for(i=0;i<n;)
      {
         if(ClockBytes > 0)
         {
            BYTE m;

            m = n-i;
            if(ClockBytes < m) m = ClockBytes;
            ClockBytes -= m;
            i += m;
         
            if(WriteOnly) /* Shift out 8 bits from d */
            {
               while(m--) ShiftOut(XAUTODAT1);
            }
            else /* Shift in 8 bits at the other end  */
            {
               while(m--) OutputByte(ShiftInOut(XAUTODAT1));
            }
        }
        else
        {
            BYTE d = XAUTODAT1;
            WriteOnly = (d & bmBIT6) ? FALSE : TRUE;

            if(d & bmBIT7)
            {
               /* Prepare byte transfer, do nothing else yet */

               ClockBytes = d & 0x3F;
            }
            else
            {
               /* Set state of output pins */

               TCK = (d & bmBIT0) ? 1 : 0;
               TMS = (d & bmBIT1) ? 1 : 0;
               TDI = (d & bmBIT4) ? 1 : 0;

               /* Optionally read state of input pins and put it in output buffer */

               if(!WriteOnly) OutputByte(2|TDO);
            };
            i++;
         };
      };

      SYNCDELAY;
      EP2BCL = 0x80; // Re-arm endpoint 2
   };
}

#endif

BOOL TD_Suspend(void)          // Called before the device goes into suspend mode
{
   return(TRUE);
}

BOOL TD_Resume(void)          // Called after the device resumes
{
   return(TRUE);
}

//-----------------------------------------------------------------------------
// Device Request hooks
//   The following hooks are called by the end point 0 device request parser.
//-----------------------------------------------------------------------------

#ifdef DEBUG
void show_request(char *which)
{
  char i;
  printf("%s:", which);
  for(i=0;i<8;i++) printf(" %02X", SETUPDAT[i]);
  printf("\n");
}
#endif

BOOL DR_GetDescriptor(void)
{
#ifdef DEBUG
   show_request("GD");
#endif
   return(TRUE);
}

BOOL DR_SetConfiguration(void)   // Called when a Set Configuration command is received
{
#ifdef DEBUG
   show_request("SC");
#endif
   Configuration = SETUPDAT[2];
   return(TRUE);            // Handled by user code
}

BOOL DR_GetConfiguration(void)   // Called when a Get Configuration command is received
{
#ifdef DEBUG
   show_request("GC");
#endif
   EP0BUF[0] = Configuration;
   EP0BCH = 0;
   EP0BCL = 1;
   return(TRUE);            // Handled by user code
}

BOOL DR_SetInterface(void)       // Called when a Set Interface command is received
{
#ifdef DEBUG
   show_request("SI");
#endif
   AlternateSetting = SETUPDAT[2];
   return(TRUE);            // Handled by user code
}

BOOL DR_GetInterface(void)       // Called when a Set Interface command is received
{
#ifdef DEBUG
   show_request("GI");
#endif
   EP0BUF[0] = AlternateSetting;
   EP0BCH = 0;
   EP0BCL = 1;
   return(TRUE);            // Handled by user code
}

BOOL DR_GetStatus(void)
{
#ifdef DEBUG
    //show_request("GS");
#endif
   if(SETUPDAT[0]==0x40)
   {
      Running = TRUE;
      return FALSE;
   };

   return(TRUE);
}

BOOL DR_ClearFeature(void)
{
#ifdef DEBUG
   show_request("CF");
#endif
   return(TRUE);
}

BOOL DR_SetFeature(void)
{
#ifdef DEBUG
   show_request("SF");
#endif
   return(TRUE);
}

BOOL DR_VendorCmnd(void)
{
#ifdef DEBUG
   show_request("VC");
#endif

   switch(SETUPDAT[1])
   {
     case 0x00: // 0x40, 0x00: reset
     {
       VC_Reset_IN();
       VC_Reset_OUT();
       break;
     }
     // 0x40, 0x01: set modem control
     // 0x40, 0x02: set flow control
     // 0x40, 0x03: set baud rate
     // 0x40, 0x04: set line property
     case 0x05: // 0xC0, 0x05: ? get status? reset?
     {
#if 0
        VC_Reset_IN();
        VC_Reset_OUT();
#endif
        EP0BUF[0] = 0x36;
        EP0BUF[1] = 0x83;
        EP0BCH = 0;
        EP0BCL = 2; // Arm endpoint with # bytes to transfer
        break;
     }
     // 0x40, 0x09: set latency timer
     // 0xC0, 0x0A: get latency timer
     // 0x40, 0x0B: set bitbang mode (mode << 8)
     // 0xC0, 0x0C: get pins
     case 0x90: // 0xC0, 0x90: read eeprom ([4] has word addr)
     {
        BYTE addr = (SETUPDAT[4]<<1) & 0x7F;
        EP0BUF[0] = PROM[addr];
        EP0BUF[1] = PROM[addr+1];
        EP0BCH = 0;
        EP0BCL = 2; // Arm endpoint with # bytes to transfer
        break;
     }
     // 0x40, 0x91: write eeprom
     // 0x40, 0x92: erase eeprom
     default:
     {
        if(SETUPDAT[0] == 0xC0)
        {
          EP0BUF[0] = 0x36;
          EP0BUF[1] = 0x83;
          EP0BCH = 0;
          EP0BCL = 2; // Arm endpoint with # bytes to transfer
        };
     }
   };

   EP0CS |= bmHSNAK; // Acknowledge handshake phase of device request

   return(FALSE); // no error; command handled OK
}

//-----------------------------------------------------------------------------
// USB Interrupt Handlers
//   The following functions are called by the USB interrupt jump table.
//-----------------------------------------------------------------------------

// Setup Data Available Interrupt Handler
void ISR_Sudav(void) INTERRUPT_0
{
#ifdef DEBUG_IRQ
   putchar('S');
#endif
   GotSUD = TRUE;            // Set flag
   EZUSB_IRQ_CLEAR();
   USBIRQ = bmSUDAV;         // Clear SUDAV IRQ
}

// Setup Token Interrupt Handler
void ISR_Sutok(void) INTERRUPT_0
{
#ifdef DEBUG_IRQ
   putchar('O');
#endif
   EZUSB_IRQ_CLEAR();
   USBIRQ = bmSUTOK;         // Clear SUTOK IRQ
}

void ISR_Sof(void) INTERRUPT_0
{
#ifdef DEBUG_IRQ
   putchar('F');
#endif
   EZUSB_IRQ_CLEAR();
   USBIRQ = bmSOF;            // Clear SOF IRQ
}

void ISR_Ures(void) INTERRUPT_0
{
#ifdef DEBUG_IRQ
   putchar('R');
#endif
   if (EZUSB_HIGHSPEED())
   {
      pConfigDscr = pHighSpeedConfigDscr;
      pOtherConfigDscr = pFullSpeedConfigDscr;
   }
   else
   {
      pConfigDscr = pFullSpeedConfigDscr;
      pOtherConfigDscr = pHighSpeedConfigDscr;
   }
   
   EZUSB_IRQ_CLEAR();
   USBIRQ = bmURES;         // Clear URES IRQ
}

void ISR_Susp(void) INTERRUPT_0
{
#ifdef DEBUG_IRQ
   putchar('P');
#endif
   Sleep = TRUE;
   EZUSB_IRQ_CLEAR();
   USBIRQ = bmSUSP;
}

void ISR_Highspeed(void) INTERRUPT_0
{
#ifdef DEBUG_IRQ
   putchar('H');
#endif
   if (EZUSB_HIGHSPEED())
   {
      pConfigDscr = pHighSpeedConfigDscr;
      pOtherConfigDscr = pFullSpeedConfigDscr;
   }
   else
   {
      pConfigDscr = pFullSpeedConfigDscr;
      pOtherConfigDscr = pHighSpeedConfigDscr;
   }

   EZUSB_IRQ_CLEAR();
   USBIRQ = bmHSGRANT;
}
void ISR_Ep0ack(void) INTERRUPT_0
{
}
void ISR_Stub(void) INTERRUPT_0
{
}
void ISR_Ep0in(void) INTERRUPT_0
{
}
void ISR_Ep0out(void) INTERRUPT_0
{
}
void ISR_Ep1in(void) INTERRUPT_0
{
}
void ISR_Ep1out(void) INTERRUPT_0
{
}
void ISR_Ep2inout(void) INTERRUPT_0
{
}
void ISR_Ep4inout(void) INTERRUPT_0
{
}
void ISR_Ep6inout(void) INTERRUPT_0
{
}
void ISR_Ep8inout(void) INTERRUPT_0
{
}
void ISR_Ibn(void) INTERRUPT_0
{
}
void ISR_Ep0pingnak(void) INTERRUPT_0
{
}
void ISR_Ep1pingnak(void) INTERRUPT_0
{
}
void ISR_Ep2pingnak(void) INTERRUPT_0
{
}
void ISR_Ep4pingnak(void) INTERRUPT_0
{
}
void ISR_Ep6pingnak(void) INTERRUPT_0
{
}
void ISR_Ep8pingnak(void) INTERRUPT_0
{
}
void ISR_Errorlimit(void) INTERRUPT_0
{
}
void ISR_Ep2piderror(void) INTERRUPT_0
{
}
void ISR_Ep4piderror(void) INTERRUPT_0
{
}
void ISR_Ep6piderror(void) INTERRUPT_0
{
}
void ISR_Ep8piderror(void) INTERRUPT_0
{
}
void ISR_Ep2pflag(void) INTERRUPT_0
{
}
void ISR_Ep4pflag(void) INTERRUPT_0
{
}
void ISR_Ep6pflag(void) INTERRUPT_0
{
}
void ISR_Ep8pflag(void) INTERRUPT_0
{
}
void ISR_Ep2eflag(void) INTERRUPT_0
{
}
void ISR_Ep4eflag(void) INTERRUPT_0
{
}
void ISR_Ep6eflag(void) INTERRUPT_0
{
}
void ISR_Ep8eflag(void) INTERRUPT_0
{
}
void ISR_Ep2fflag(void) INTERRUPT_0
{
}
void ISR_Ep4fflag(void) INTERRUPT_0
{
}
void ISR_Ep6fflag(void) INTERRUPT_0
{
}
void ISR_Ep8fflag(void) INTERRUPT_0
{
}
void ISR_GpifComplete(void) INTERRUPT_0
{
}
void ISR_GpifWaveform(void) INTERRUPT_0
{
}
