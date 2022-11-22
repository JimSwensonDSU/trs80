/*
 *
 * Utility for generating the audio to load a CAS emulator file.
 *
 * These files are a byte capture of the individual bytes for
 * the Machine Language Object (SYSTEM) Tape format.
 *
 * See the cassette_system() function in cassette_port_write.c for
 * details on the SYSTEM format itself.
 *
 * 1. On TRS-80
 *
 *    >SYSTEM
 *    *? PGMNAME
 *
 *    Where PGMNAME is the appropriate name for the program
 *
 * 2. Run this program on laptop, which will use write_byte() to transfer
 *    over the machine code from the CAS file to the TRS-80.
 *
 *    $ load_cas file.cas
 *
 * 3. On TRS-80
 *
 *    *? /
 *
 *    Or whatever input is needed for the program.
 *
 * Audio Port settings on C Laptop side are important.  Built in headphone/mic jack
 * not reliable.  Not enough amplitude on pulses.  Using a usb adapter.
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

int initialize(int *file_descriptor);
int write_hex_string(int fd, char *s);
int write_byte(int fd, unsigned char c);
void flush(int fd);


#define SOUND_PCM_WRITE_BITS ( 1610895365 )
#define SOUND_PCM_WRITE_CHANNELS ( 1610895366 )
#define SOUND_PCM_WRITE_RATE ( 1610895362 )
#define SOUND_PCM_SYNC ( 20481 )

#define RATE 11025
#define SIZE 8      /* sample size: 8 or 16 bits */
#define CHANNELS 1  /* 1 = mono 2 = stereo */


int main(int argc, char *argv[])
{
  int fd, fd_cas;
  unsigned char c;

  if (argc != 2)
  {
     printf("Usage: %s file.cas\n", argv[0]);
     exit(1);
  }

  if (initialize(&fd) < 0)
  {
     perror("Fail");
     exit(1);
  }

  /* open sound device */
  fd_cas = open(argv[1], O_RDONLY);
  if (fd_cas < 0)
  {
     perror("Unable to open file");
     perror(argv[1]);
     exit(1);
  }

  while (read(fd_cas,&c,1))
  {
     write_byte(fd,c);
  }

  close(fd_cas);

  flush(fd);
  close(fd);
}

/*
 * Set up audio port for output.
 */
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


/*
 * Sends bytes represented by 2 digit hex values in a string.
 */
int write_hex_string(int fd, char *s)
{
   char *p;
   unsigned char x;

   for (p=s; *p; p+=2)
   {
      x=(((*p&0x40)?9+(*p&0x07):(*p&0x0f))<<4)|((*(p+1)&0x40)?9+(*(p+1)&0x07):(*(p+1)&0x0f));

      if (write_byte(fd, x)<0)
      {
         perror("Write hex string byte non literal failed");
         return(-1);
      }
   }

   return(0);
}


/*
 * Send individual byte.
 */
int write_byte(int fd, unsigned char c)
{
   int i;
   unsigned char x;
   unsigned char *bit, *p;
   int status;

   char *bit1 = "80ff0080808080808080808080ff00808080808080808080";
   char *bit0 = "80ff00808080808080808080808080808080808080808080";

   for (i=7; i>=0; i--)
   {
      if ((c>>i)&1)
      {
         bit = bit1;
      }
      else
      {
         bit = bit0;
      }

      for (p=bit; *p; p+=2)
      {
         x=(((*p&0x40)?9+(*p&0x07):(*p&0x0f))<<4)|((*(p+1)&0x40)?9+(*(p+1)&0x07):(*(p+1)&0x0f));
         status = write(fd, &x, 1);
         if (status != 1)
         {
            perror("wrote wrong number of bytes");
            return(-1);
         }
      }
   }

   return(0);
}

/* extra stuff to flush the descriptor out */
void flush(int fd)
{
   write_hex_string(fd, "00000000000000000000");
}
