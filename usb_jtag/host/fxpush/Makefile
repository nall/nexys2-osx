CFLAGS=-Wall -O2

LIBUSB  = /cygdrive/c/Programme/LibUSB-Win32
CFLAGS += -Wall -O2 -I${LIBUSB}/include -L${LIBUSB}/lib/gcc

list: fxpush
	./fxpush :

boot: fxpush
	./fxpush blaster.hex d06 9fb

fxpush: fxpush.c
	$(CC) $(CFLAGS) -o fxpush fxpush.c -lusb
