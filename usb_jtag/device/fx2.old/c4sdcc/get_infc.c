#include "c4sdcc.h"

INTRFCDSCR xdata*	EZUSB_GetIntrfcDscr(BYTE ConfigIdx, BYTE IntrfcIdx, BYTE AltSetting)
{
	CONFIGDSCR xdata *config_dscr;
	INTRFCDSCR xdata *intrfc_dscr;

	if(config_dscr = EZUSB_GetConfigDscr(ConfigIdx))
	{
		intrfc_dscr = (INTRFCDSCR xdata *)((WORD)config_dscr + config_dscr->length);
		while((intrfc_dscr->type == INTRFC_DSCR) || (intrfc_dscr->type == ENDPNT_DSCR))
		{
			if(intrfc_dscr->type == INTRFC_DSCR)
				if((intrfc_dscr->index == IntrfcIdx) && (intrfc_dscr->alt_setting == AltSetting))
					return(intrfc_dscr);
			intrfc_dscr = (INTRFCDSCR xdata *)((WORD)intrfc_dscr + intrfc_dscr->length);
		}
	}

	return 0;
}
