#include "c4sdcc.h"

void EZUSB_Discon(BOOL renum)
{
   if(renum)                                 // If renumerate (i.e. 8051 will handle SETUP commands)
      USBCS |= (bmDISCON | bmRENUM);        // disconnect from USB and set the renumerate bit
   else
      USBCS |= bmDISCON;                     // just disconnect from USB
		
   EZUSB_Delay(1500);      // Wait 1500 ms

   USBIRQ = 0xff;          // Clear any pending USB interrupt requests.  They're for our old life.
   EPIRQ = 0xff;
   EZUSB_IRQ_CLEAR();

   USBCS &=~bmDISCON;      // reconnect USB
}

