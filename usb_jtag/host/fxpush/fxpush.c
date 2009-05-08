/*
   Utility to upload HEX file to FX2/FX target using libusb

   Runs with Cygwin, GCC, libusb-win32 from http://libusb-win32.sourceforge.net

   The ezusb_* code here is from Stephan Meyer (ste_meyer, web.de), as posted in
     http://osdir.com/ml/lib.libusb.devel.windows/2005-02/msg00001.html or
     http://sourceforge.net/mailarchive/message.php?msg_id=667335292%40web.de
*/

#include <inttypes.h>
#include <stdio.h>
#include <usb.h>
#include <stdio.h>


#if 0
#define EZUSB_CS_ADDRESS 0x7F92 /* FX */
#else
#define EZUSB_CS_ADDRESS 0xE600 /* FX2 */
#endif

#define MAX_HEX_RECORD_LENGTH 16

typedef struct
{
  uint32_t length;
  uint32_t address;
  uint32_t type;
  uint8_t data[MAX_HEX_RECORD_LENGTH];
} hex_record;


static int ezusb_read_hex_record(hex_record *record, FILE *file); 
static int ezusb_load(usb_dev_handle *dev, uint32_t address, uint32_t length, uint8_t *data);

static int ezusb_read_hex_record(hex_record *record, FILE *file)
{
  char c;
  uint32_t i, length, read, tmp, checksum;

  if(feof(file))
  {
    return 0;
  }

  c = getc(file);

  if(c != ':')
  {
    return 0;
  }

  read = fscanf(file, "%2X%4X%2X", &record->length, &record->address, &record->type);

  if(read != 3)
  {
    return 0;
  }

  checksum = record->length + (record->address >> 8) + record->address + record->type;

  length = record->length;

  if(length > MAX_HEX_RECORD_LENGTH)
  {
    return 0;
  }

  for(i = 0; i < length; i++)
  {
    read = fscanf(file, "%2X", &tmp);

    if(read != 1)
    {
      return 0;
    }

    record->data[i] = (uint8_t)tmp;
    checksum += tmp;
  }

  read = fscanf(file, "%2X\n", &tmp);

  if((read != 1) || (((uint8_t)(checksum + tmp)) != 0x00))
  {
    return 0;
  }

  return 1;
}

static int ezusb_load(usb_dev_handle *dev, uint32_t address, uint32_t length, uint8_t *data)
{
  if(usb_control_msg(dev, 0x40, 0xA0, address, 0, (char *)data, length, 5000) <= 0)
  {
    return 0;
  }

  return 1;
}


int ezusb_load_firmware(usb_dev_handle *dev, const char *hex_file)
{
  uint8_t ezusb_cs;
  FILE *firmware;
  hex_record record;

  firmware = fopen(hex_file, "r");

  if(!firmware)
  {
    return 0;
  }

  ezusb_cs = 1;

  if(!ezusb_load(dev, EZUSB_CS_ADDRESS, 1, &ezusb_cs))
  {
    return 0;
  }

  while(ezusb_read_hex_record(&record, firmware))
  {
    if(record.type != 0)
    {
      break;
    }

    // printf("Load L=%X @=%X\n", record.length, record.address);

    if(!ezusb_load(dev, record.address, record.length, record.data))
    {
      return 0;
    }
  }

  ezusb_cs = 0;

  ezusb_load(dev, EZUSB_CS_ADDRESS, 1, &ezusb_cs);

  fclose(firmware);

  return 1;
}

#define BUFLEN 64

void show_dev(struct usb_device *dev)
{
  int j,n;
  char buf[BUFLEN];
  usb_dev_handle *dh = NULL; /* the device handle */

  printf("found %04X:%04X", dev->descriptor.idVendor, dev->descriptor.idProduct);

  dh = usb_open(dev);
  if(dh)
  {
    int i;
    i = usb_set_configuration(dh, 1);
    if(i >= 0) i = usb_claim_interface(dh, 0);
    if(i >= 0) 
    {
      n = usb_get_string_simple(dh, dev->descriptor.iManufacturer, buf, BUFLEN);
      if(n>0) { printf(" M'"); for(j=0;j<n;j++) putchar(buf[j]); putchar('\''); };
      n = usb_get_string_simple(dh, dev->descriptor.iProduct, buf, BUFLEN);
      if(n>0) { printf(" P'"); for(j=0;j<n;j++) putchar(buf[j]); putchar('\''); };
      n = usb_get_string_simple(dh, dev->descriptor.iSerialNumber, buf, BUFLEN);
      if(n>0) { printf(" S'"); for(j=0;j<n;j++) putchar(buf[j]); putchar('\''); };
      usb_release_interface(dh, 0);
    }
    usb_close(dh);
  }
  putchar('\n');
}

int main(int argc, char *argv[])
{
  usb_dev_handle *dh = NULL; /* the device handle */
  struct usb_device *fd = NULL;
  struct usb_bus *bus;
  int listmode = 0;

  if(argc==2 && argv[1][0] == ':')
  {
    listmode = 1;
  }
  else if(argc<3)
  {
    fprintf(stderr, "syntax: fxpush {hexfile} {vid:pid}\n");
    fprintf(stderr, "vid or pid or both may be empty (= don't care)\n");
    fprintf(stderr, "multiple hex vid:pid pairs may be specified\n");
    return -1;
  };

  usb_init(); /* initialize the library */
  usb_find_busses(); /* find all busses */
  usb_find_devices(); /* find all connected devices */

  for(bus = usb_get_busses(); bus && !fd; bus = bus->next)
  {
    struct usb_device *dev;

    for(dev = bus->devices; dev && !fd; dev = dev->next)
    {
      int i;
      if(listmode)
      {
        show_dev(dev);
      }
      else for(i=2; i<argc && !fd; i++)
      {
        char *sep;
        unsigned long vid, pid;
        vid = strtoul(argv[i], &sep, 16);
        if(sep == argv[i] || dev->descriptor.idVendor == vid)
        {
          if(*sep == ':') 
          {
            pid = strtoul(sep+1, NULL, 16);
            if(dev->descriptor.idProduct == pid)
            {
              show_dev(dev);
              fd = dev;
            }
          }
          else
          {
            show_dev(dev);
            fd = dev;
          }
        }
      }
    }
  };

  if(listmode)
  {
    return 0;
  };

  if(!fd)
  {
    printf("error: device not found!\n");
    return -1;
  }

  dh = usb_open(fd);
  if(!ezusb_load_firmware(dh, argv[1]))
  {
    printf("error: ezusb_load_firmware failed\n");
    return -1;
  }
  usb_release_interface(dh, 0);
  usb_close(dh);

  return 0;
}
