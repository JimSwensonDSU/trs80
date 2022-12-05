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
int write_hex_string(int fd, char *s, int literal);
int write_wav_header(int fd, int n);
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

  if (argc != 2 && argc != 3)
  {
     printf("Usage: %s file.cas [file.wav]\n", argv[0]);
     exit(1);
  }

  if (argc == 2)
  {
     if (initialize(&fd) < 0)
     {
        perror("Fail");
        exit(1);
     }
  }
  else
  {
     /* Writing to a file instead */
     fd = open(argv[2], O_WRONLY|O_CREAT|O_TRUNC, S_IWUSR|S_IRUSR);
     if (fd < 0)
     {
        perror("Fail");
        exit(1);
     }
  }

  /* open CAS file */
  fd_cas = open(argv[1], O_RDONLY);
  if (fd_cas < 0)
  {
     perror("Unable to open file");
     perror(argv[1]);
     exit(1);
  }

  if (argc == 3)
  {
     struct stat buf;
     fstat(fd_cas, &buf);
     off_t size = buf.st_size;

     /* Write WAV header */
     write_wav_header(fd, size);
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
int write_hex_string(int fd, char *s, int literal)
{
   char *p;
   unsigned char x;

   for (p=s; *p; p+=2)
   {
      x=(((*p&0x40)?9+(*p&0x07):(*p&0x0f))<<4)|((*(p+1)&0x40)?9+(*(p+1)&0x07):(*(p+1)&0x0f));

      if (literal)
      {
         if (write(fd, &x, 1)!=1)
         {
            perror("Write hex string byte literal failed");
            return(-1);
         }
      }
      else
      {
         if (write_byte(fd, x)<0)
         {
            perror("Write hex string byte non literal failed");
            return(-1);
         }
      }

   }

   return(0);
}


int write_wav_header(int fd, int n)
{

/*
 * 00000000  52 49 46 46 XX XX XX XX  57 41 56 45 66 6d 74 20  |RIFF....WAVEfmt |
 * 00000010  10 00 00 00 01 00 01 00  11 2b 00 00 11 2b 00 00  |.........+...+..|
 * 00000020  01 00 08 00 64 61 74 61  YY YY YY YY              |....data....|
 *
 * XX XX XX XX = (192*bytesin + 36), LSB first
 * YY YY YY YY = (192*bytesin), LSB first
 */

   char *header = "52494646%02x%02x%02x%02x57415645666d74201000000001000100112b0000112b00000100080064617461%02x%02x%02x%02x";
   char buf[100];
   int count;

   n *= 192;
   count = n + 36;
   sprintf(buf,header,count&0xff,(count>>8)&0xff,(count>>16)&0xff,(count>>24)&0xff,n&0xff,(n>>8)&0xff,(n>>16)&0xff,(n>>24)&0xff);

   return(write_hex_string(fd, buf, 1));
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
   write_hex_string(fd, "00000000000000000000", 0);
}
