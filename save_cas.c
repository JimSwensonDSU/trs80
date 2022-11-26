/*
 *
 * Utility to capture anything coming from cassette port byte by byte.
 *
 * 1. Run this program on laptop, which will call read_byte() with wait=1 to get
 *    the first byte.  This is a blocking read.  Thereafter it will set wait=0.
 *    See READ_LIMIT define for how many loops it will read until it gives up
 *    waiting for the next byte.  i.e. this program expects data to come in without
 *    a pause.
 *
 *    A hexdump of read bytes will be printed to stdout.
 *
 *    $ save_cas [file.cas]
 *
 *    where file.cas is an optional file to save the bytes to, suitable for using
 *    the load_cas program to send back to the TRS-80.
 *
 * 2. On TRS-80
 *
 *    Initiate a CSAVE, for example
 *    >CSAVE "STUFF"
 *
 *    or something else that generates data out to the cassette port.  But note
 *    the comment regarding READ_LIMIT above.
 *
 * Port settings on C side are important.  Built in headphone/mic jack not reliable.  Not enough
 * amplitude on pulses.  Using a usb adapter.
 *
 * Port settings using USB Plugable:
 *
 * Speaker: Level 100%
 * Mic:     Level 31%
 *          Enhancements: Disabled
 *
 */
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>


/*
#define DEBUG 1
*/

#define READ_LIMIT 500
#define READ_AHEAD 10
#define INITIAL_SKIP 0
#define BURN 5
#define PULSE 170
#define SYNC_BYTE 165

#define SOUND_PCM_WRITE_BITS 1610895365
#define SOUND_PCM_WRITE_CHANNELS 1610895366
#define SOUND_PCM_WRITE_RATE 1610895362
#define SOUND_PCM_SYNC 20481

#define RATE 11025
#define SIZE 8      /* sample size: 8 or 16 bits */
#define CHANNELS 1  /* 1 = mono 2 = stereo */

#define DUMP_BYTES 16

int initialize(int *file_descriptor);
int read_byte(int fd, int wait, unsigned char *c, int initial_skip);
void dump_line(int address, unsigned char c_line[], int num_bytes);

int initialize(int *file_descriptor)
{
   int fd;
   int arg;
   int status;

   /* open sound device */
   fd = open("/dev/dsp", O_RDWR);
   if (fd < 0)
   {
      perror("open of /dev/dsp failed");
      return(-1);
   }

   /* set sampling parameters */
   arg = SIZE;      /* sample size */
   status = ioctl(fd, SOUND_PCM_WRITE_BITS, &arg);
   if (status == -1)
   {
      perror("SOUND_PCM_WRITE_BITS ioctl failed");
      return(-1);
   }

   if (arg != SIZE)
   {
      perror("unable to set sample size");
      return(-1);
   }

   arg = CHANNELS;  /* mono or stereo */
   status = ioctl(fd, SOUND_PCM_WRITE_CHANNELS, &arg);
   if (status == -1)
   {
      perror("SOUND_PCM_WRITE_CHANNELS ioctl failed");
      return(-1);
   }

   if (arg != CHANNELS)
   {
      perror("unable to set number of channels");
      return(-1);
   }


   arg = RATE;      /* sampling rate */
   status = ioctl(fd, SOUND_PCM_WRITE_RATE, &arg);
   if (status == -1)
   {
      perror("SOUND_PCM_WRITE_RATE ioctl failed");
      return(-1);
   }


   *file_descriptor = fd;
   return(0);
}

int read_byte(int fd, int wait, unsigned char *c, int initial_skip)
{
   unsigned char buf;
   int num_read = 0;
   int i,j;
   int bit_started = 0;
   int byte = 0;
   int bits = 0;
   int skip = initial_skip;

   while (read(fd, &buf, 1) == 1)
   {
      num_read++;
#if defined(DEBUG)
printf("Read: %d\n", buf);
#endif

      if ((num_read>READ_LIMIT) && (!wait))
      {
#if defined(DEBUG)
         perror("Exceeded READ_LIMIT");
#endif
         return(-1);
      }

      if (skip > 0)
      {
         skip--;
         continue;
      }

      j = (buf>=PULSE) ? 1 : 0;

      if (bit_started)
      {
#if defined(DEBUG)
printf("Checking: %d\n", j);
#endif
         bit_started = 0;
         byte = byte*2 + j;
         bits++;
         if (bits >= 8)
         {
            *c = byte;
#if defined(DEBUG)
printf("Byte: %d %d\n", byte, num_read);
#endif
            /* Burn off */
            for (i=0; i<BURN; i++)
            {
               read(fd, &buf, 1);
            }
            return(0);
         }
         skip = READ_AHEAD-1;
      }
      else
      {
         if (j)
         {
#if defined(DEBUG)
printf("Bit started\n");
#endif
            bit_started = 1;
            skip = READ_AHEAD;
            read(fd, &buf, 1);
#if defined(DEBUG)
printf("Read ahead: %d\n", buf);
#endif
            if (buf < PULSE) {skip--;}
         }
      }
   }

   perror("read failed");
   return(-1);
}

/* 00000000  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................| */
void dump_line(int address, unsigned char c_line[], int num_bytes)
{
   int i;

   printf("%08x ", address);
   for (i=0; i<num_bytes; i++)
   {
      printf(" %02x", c_line[i]);
      if (i == 7) printf(" ");
   }
   for (i=num_bytes; i<DUMP_BYTES; i++)
   {
      printf("   ");
      if (i == 7) printf(" ");
   }
   printf("  |");
   for (i=0; i<num_bytes; i++)
   {
      printf("%c", c_line[i]>=32 && c_line[i]<127 ? c_line[i] : '.');
   }
   for (i=num_bytes; i<DUMP_BYTES; i++)
   {
      printf(" ");
   }
   printf("|\n");
}


int main(int argc, char *argv[])
{
  int fd;
  int save_fd = -1;
  int wait = 1;
  unsigned char c;
  unsigned char c_line[DUMP_BYTES];
  int num_bytes = 0;
  int address = 0;

  if (argc>1)
  {
     if ((save_fd=open(argv[1], O_WRONLY|O_CREAT|O_TRUNC, S_IWUSR|S_IRUSR)) < 0)
     {
        perror("Unable to open output file");
	exit(1);
     }
  }

  if (initialize(&fd) < 0)
  {
     perror("Fail");
     exit(1);
  }


  while (read_byte(fd, wait, &c, 0) == 0)
  {
     wait = 0;
     if (save_fd != -1) if (write(save_fd,&c,1)!=1) perror("write fail");
     if (num_bytes == DUMP_BYTES)
     {
        dump_line(address, c_line, num_bytes);
	address += DUMP_BYTES;
	num_bytes = 0;
     }
     c_line[num_bytes++] = c;
  }

  if (num_bytes > 0)
  {
     dump_line(address, c_line, num_bytes);
  }

  if (save_fd != -1) close(save_fd);
  close(fd);
}
