#include "c4sdcc.h"

#define GD_DEPTH	3		// Depth of the Get Descriptor nesting

// NOTE: This function is psuedo re-entrant
DSCR xdata* EZUSB_GetDscr(BYTE index, DSCR *start_dscr, BYTE type)
{
	static DSCR xdata*	dscr[GD_DEPTH];
	static BYTE		end_type[GD_DEPTH];
	DSCR xdata *			prev_dscr;

	if(start_dscr)
	{
		dscr[index] = (DSCR xdata*) start_dscr;
		end_type[index] = start_dscr->type;
		dscr[index] = (DSCR xdata *)((WORD)dscr[index] + dscr[index]->length);
	}

	while(dscr[index]->type && (dscr[index]->type != end_type[index]))
	{
		if(dscr[index]->type == type)
		{
			prev_dscr = dscr[index];
			dscr[index] = (DSCR xdata *)((WORD)dscr[index] + dscr[index]->length);
			return(prev_dscr);
		}
		dscr[index] = (DSCR xdata *)((WORD)dscr[index] + dscr[index]->length);
	}

	return 0;
}
