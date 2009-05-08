#include "c4sdcc.h"

void EZUSB_Resume(void)
{
    if(EZUSB_EXTWAKEUP())
    {
        USBCS |= bmSIGRESUME;
        EZUSB_Delay(20);
        USBCS &= ~bmSIGRESUME;
    }
}
