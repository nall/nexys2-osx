#include "c4sdcc.h"

void putchar(char c)
{
  while(!TI1);
  TI1 = 0;
  SBUF1 = c;
  if(c==10) putchar(13);
}

