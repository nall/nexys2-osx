#include "c4sdcc.h"

STRINGDSCR xdata *	EZUSB_GetStringDscr(BYTE StrIdx)
{
	STRINGDSCR xdata *	dscr;

	dscr = (STRINGDSCR xdata *) pStringDscr;

	while(dscr->type == STRING_DSCR)
	{
		if(!StrIdx--)
			return(dscr);
		dscr = (STRINGDSCR xdata *)((WORD)dscr + dscr->length);
	}

	return 0;
}
