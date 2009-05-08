#include "c4sdcc.h"

BOOL EZUSB_ReadI2C(BYTE addr, BYTE length, BYTE xdata *dat)
{
	EZUSB_ReadI2C_(addr, length, dat);

	while(TRUE)
		switch(I2CPckt.status)
		{
			case I2C_IDLE:
				return(I2C_OK);
			case I2C_NACK:
				I2CPckt.status = I2C_IDLE;
				return(I2C_NACK);
			case I2C_BERROR:
				I2CPckt.status = I2C_IDLE;
				return(I2C_BERROR);
		}
}

BOOL EZUSB_WriteI2C(BYTE addr, BYTE length, BYTE xdata *dat)
{
	EZUSB_WriteI2C_(addr, length, dat);

	while(TRUE)
		switch(I2CPckt.status)
		{
			case I2C_IDLE:
				return(I2C_OK);
			case I2C_NACK:
				I2CPckt.status = I2C_IDLE;
				return(I2C_NACK);
			case I2C_BERROR:
				I2CPckt.status = I2C_IDLE;
				return(I2C_BERROR);
		}
}
