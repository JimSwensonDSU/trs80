/*
 *
 * Sample code to show read/write via cassette port.
 * A BASIC listing is included for the TRS-80 side.
 *
 * cassette_system() will write that BASIC program to the cassette port, suitable for loading via SYSTEM.
 *
 * 1. On TRS-80
 *
 *    >SYSTEM
 *    *? CS
 *
 * 2. Run this pgm on laptop, which will run cassette_system() and then fall into its read/echo loop.
 *
 * 3. On TRS-80
 *
 *    *? /
 *    >RUN
 *
 *
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
#define LEADER_BYTE 0
#define LEADER_LENGTH 255
#define SYNC_BYTE 165
#define NUM_END_STRING_BYTE 10
#define END_STRING_BYTE 13
#define DATA_BLOCK_MAX 100

#define HEARTBEAT "!!HEARTBEAT!!"
#define LINE_LENGTH 62

#define SOUND_PCM_WRITE_BITS 1610895365
#define SOUND_PCM_WRITE_CHANNELS 1610895366
#define SOUND_PCM_WRITE_RATE 1610895362
#define SOUND_PCM_SYNC 20481

#define RATE 11025
#define SIZE 8      /* sample size: 8 or 16 bits */
#define CHANNELS 1  /* 1 = mono 2 = stereo */

int initialize(int *file_descriptor);
int read_string(int fd, char *s, int n);
int read_byte(int fd, int wait, unsigned char *c, int initial_skip);
int write_hex_string(int fd, char *s, int literal);
int write_wav_header(int fd, int n);
int write_string(int fd, char *s);
int write_byte(int fd, unsigned char c);
int cassette_system(int fd);

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

int read_string(int fd, char *s, int n)
{
   unsigned char c;
   int zeros = 0;
   int inx = 0;

   if (read_byte(fd, 1, &c, INITIAL_SKIP) < 0)
   {
      perror("read_byte failed 1");
      return(-1);
   }

   while (c == LEADER_BYTE)
   {
      zeros++;
      if (read_byte(fd, 0, &c, 0) < 0)
      {
         perror("read_byte failed 2");
         return(-1);
      }
   }

   if (zeros < LEADER_LENGTH)
   {
      perror("missing leader");
      return(-1);
   }

   if (c != SYNC_BYTE)
   {
      perror("missing sync");
      return(-1);
   }

   while (read_byte(fd, 0, &c, 0) == 0)
   {
      if ((inx+1) > n)
      {
         perror("string too long");
         return(-1);
      }

      if (c == END_STRING_BYTE)
      {
         *(s+inx) = '\0';
         return(0);
      }

      *(s+inx) = c;
      inx++;
   }

   perror("read_byte failed 3");
   return(-1);
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
         perror("Exceeded READ_LIMIT");
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

int write_string(int fd, char *s)
{
   int i;
   char *p;

   for (i=0; i<LEADER_LENGTH; i++)
   {
      if (write_byte(fd, LEADER_BYTE)<0)
      {
         perror("Write LEADER_BYTE failed");
         return(-1);
      }
   }

   if (write_byte(fd, SYNC_BYTE)<0)
   {
      perror("Write SYNC_BYTE failed");
      return(-1);
   }

   for (p=s; *p; p++)
   {
      if (write_byte(fd, *p)<0)
      {
         perror("Write string char failed");
         return(-1);
      }
   }

   for (i=0; i<NUM_END_STRING_BYTE; i++)
   {
      if (write_byte(fd, END_STRING_BYTE)<0)
      {
         perror("Write END_STRING_BYTE failed");
         return(-1);
      }
   }
}


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

/*
         status = ioctl(fd, SOUND_PCM_SYNC, 0);
         if (status == -1)
         {
            perror("SOUND_PCM_SYNC ioctl failed");
            return(-1);
         }
*/
      }
   }

   return(0);
}


int main(int argc, char *argv[])
{
  int fd;
  char buf[1000];
  char buf2[1000];
  int readfd=-1, writefd=-1;

  char message[100][LINE_LENGTH+1];
  int message_cnt = 0;
  int inx;
  int max_message = sizeof(message)/sizeof(message[0]);

  if (argc!=1 && argc!=3)
  {
     printf("Usage: %s [ [readfifo] [writefifo] ]\n", argv[0]);
     exit(1);
  }

  if (initialize(&fd) < 0)
  {
     perror("Fail");
     exit(1);
  }

  /* Open the FIFOs */
  if (argc==3)
  {
     if ( (readfd = open(argv[1], O_RDONLY|O_NONBLOCK)) < 0 )
     {
        printf("Unable to open %s for read (%d)\n", argv[1], errno);
        exit(1);
     }
     if ( (writefd = open(argv[2], O_WRONLY)) < 0 ) /* Will block until there is a reader */
     {
        printf("Unable to open %s for write (%d)\n", argv[2], errno);
        exit(1);
     }
  }

  /* Load the BASIC program client */
  system("date");
  if (cassette_system(fd) < 0)
  {
     exit(1);
  }
  system("date");


  while (1)
  {
     /* Read from client */
     if (read_string(fd, buf, sizeof(buf)) < 0)
     {
        perror("read_string fail");
        exit(1);
     }
     else
     {
        printf("Read from client: >%s<\n", buf);

        if (readfd == -1)
        {
           /* Just echo back */
           strcpy(message[0],buf);
           message_cnt = 1;
        }
        else
        {
           /* Send string to FIFO, except for heartbeats */
           if (strcmp(HEARTBEAT,buf))
           {
               write(writefd,buf,strlen(buf)+1);
           }


           // Create the structures for select()
           struct timeval tv;
           fd_set in_set, out_set;
           int maxfd = 0;

           // Wait 0.25 sec for the events - you can wait longer if you want to, but the library has internal timeouts
           // so it needs to be called periodically even if there are no network events
           tv.tv_usec = 250000;
           tv.tv_sec = 0;

           /* Check for waiting data on FIFO */

           // Initialize the sets
           FD_ZERO (&in_set);
           FD_ZERO (&out_set);

           // Add your own descriptors you need to wait for, if any
           FD_SET(readfd, &in_set);
           maxfd = readfd;

           // Call select()
           puts("Calling select");
           if ( select (maxfd + 1, &in_set, &out_set, 0, &tv) < 0 )
           {
              // Error
              puts("select error");
              exit(1);
           }
           puts("After select");

           if (FD_ISSET(readfd, &in_set)) {
              char response[1000];
              int n = read(readfd,response,sizeof(response));
              char *p = response;

              puts("In FD_ISSET");
              printf("Read %d bytes\n", n);

              /* Assume NULL terminated ... */
              while (p<response+n)
              {
                 if (message_cnt >= max_message)
                 {
                    printf("Exceed message buffer size %d\n", max_message);
                    p+=(strlen(p)+1);
                 }
                 else if (*p)
                 {
                    printf("Putting >%s< into queue\n", p);
                    if (strlen(p)>LINE_LENGTH)
                    {
                       memcpy(message[message_cnt],p,LINE_LENGTH);
                       message[message_cnt][LINE_LENGTH] = '\0';
                       p+=LINE_LENGTH;
                    }
                    else
                    {
                       strcpy(message[message_cnt],p);
                       p+=(strlen(p)+1);
                    }

                    message_cnt++;
                 }
                 else
                 {
                    /* Ignore empty string */
                    p++;
                 }
              }
           }
        }


        /* Send response to client */

        if ( (message_cnt == 0) || (!strcmp(message[0],HEARTBEAT)) )
        {
           write_string(fd, HEARTBEAT);
        }
        else
        {
           char buf[100],buf2[100];
           int inx;

           sprintf(buf,"%c%s",(message_cnt>1)?'1':'0',message[0]);

           /* shift the messages */
           for (inx=0;inx<message_cnt-1;inx++)
           {
              strcpy(message[inx],message[inx+1]);
           }
           message_cnt--;

           /* Need to quote : and , */
           if (strchr(buf,':')||strchr(buf,','))
           {
              sprintf(buf2,"\"%s\"",buf);
              write_string(fd, buf2);
           }
           else
           {
              write_string(fd, buf);
           }
        }
     }
  }

  close(fd);
}

/*


Y$ is a simple assembled routine to scroll the display area up 1 line.

21 C0 3C   LD   HL, 3CC0H ; line 4 destination
11 80 3C   LD   DE, 3C80H ; line 3 source
01 c0 02   LD   BC, 02C0  ; 11 lines
ED B0      LDIR
C9         RET


1 '       0: MESSAGE LINE
2 '      64: DIVIDER LINE
2 ' 128-832: OUTPUT LINES
3 '     896: DIVIDER LINE
4 '     960: INPUT LINE

10 CLEAR(1000)
20 T=960:O=128
25 Y$=CHR$(33)+CHR$(192)+CHR$(60)+CHR$(17)+CHR$(128)+CHR$(60)+CHR$(1)+CHR$(192)+CHR$(2)+CHR$(237)+CHR$(176)+CHR$(201)
30 CLS
40 PRINT @ 64,STRING$(64,140);:PRINT @ 896,STRING$(64,140);
50 A$="":B$="":C$="":D$="":E$="":Z$=STRING$(63,32)
100 FOR I=1 TO 1000
110 A$=INKEY$
120 IF A$="" 200
130 IF A$=CHR$(8) GOSUB 2000: GOTO 200 ' BACKSPACE
140 IF A$=CHR$(13) GOSUB 3000: GOTO 200 ' HIT RETURN
150 IF A$=CHR$(31) GOTO 20
160 IF ASC(A$)<32 OR ASC(A$)>127 GOTO 200' IGNORE
170 GOSUB 4000 ' TYPED A CHAR
200 IF INT(I/20) AND 1 PRINT @ T,CHR$(143); ELSE PRINT @ T," "; ' BLINK CURSOR
210 NEXT
300 GOSUB 8000
999 GOTO 100

1000 ' MESSAGE AT TOP
1000 PRINT @ 0,Z$;
1020 PRINT @ 0,C$;
1999 RETURN

2000 ' BACKSPACE
2010 IF LEN(B$)=0 GOTO 2999
2020 PRINT @ T," ";
2030 T=T-1
2040 B$=LEFT$(B$,LEN(B$)-1)
2999 RETURN

3000 ' SEND
3010 IF LEN(B$)=0 C$="** TYPE A MSG BEFORE SENDING **":GOSUB 1000:GOTO 3999
3020 PRINT @ 960,Z$; ' CLEAR TYPING LINE
3030 D$=B$:GOSUB 6000
3040 E$=B$
3050 GOSUB 9000
3060 B$="":
3070 GOSUB 7000
3080 T=960
3999 RETURN

4000 ' TYPED A CHAR
4010 IF LEN(B$)>=62 C$="** MAX MSG LENGTH IS 62 **":GOSUB 1000:GOTO 4999
4020 PRINT @ T,A$;
4030 B$=B$+A$
4040 T=T+1
4999 RETURN

6000 ' SEND TO CASSETTE
6010 C$="** SENDING MESSAGE **":GOSUB 1000
6020 PRINT #-1,D$
6030 C$="** MESSAGE SENT **":GOSUB 1000
6999 RETURN

7000 ' RECEIVE FROM CASSETTE
7010 C$="RECEIVING MESSAGE **":GOSUB 1000
7020 INPUT #-1,D$
7030 IF LEN(D$)=0 C$="** NO MESSAGE RECEIVED **":GOSUB 1000:GOTO 7999
7040 C$="** MESSAGE RECEIVED **":GOSUB 1000
7050 IF D$="!!HEARTBEAT!!" C$="** HEARTBEAT RECEIVED **":GOSUB 1000:GOTO 7999
7060 IF LEN(D$)>63 D$=LEFT$(D$,63)
7070 E$=RIGHT(D$,LEN(D$)-1)
7080 GOSUB 9000
7090 IF LEFT$(D$,1)="1" GOSUB 8000
7999 RETURN

8000 ' HEARTBEAT
8010 C$="** SENDING HEARTBEAT **":GOSUB 1000
8020 D$="!!HEARTBEAT!!":GOSUB 6020
8030 GOSUB 7000
8999 RETURN

9000 ' DISPLAY RECEIVED STRING
9010 IF O>832 O=832:GOSUB 10000
9020 PRINT @ O,Z$;
9030 PRINT @ O,E$;
9040 O=O+64
9999 RETURN

10000 ' SCROLL DISPLAY
10010 POKE 16526,PEEK(VARPTR(Y$)+1)
10020 POKE 16527,PEEK(VARPTR(Y$)+2)
10030 X=USR(0)
10999 RETURN


The CSAVE output.  Take from 00000104 on for the SYSTEM load and
put a machine language header on instead.


00000000  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00000010  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00000020  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00000030  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00000040  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00000050  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00000060  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00000070  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00000080  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00000090  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
000000a0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
000000b0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
000000c0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
000000d0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
000000e0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
000000f0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 a5  |................|
00000100  d3 d3 d3 43 06 43 01 00  3a 93 fb 20 20 20 20 20  |...C.C..:..     |
00000110  20 30 3a 20 4d 45 53 53  41 47 45 20 4c 49 4e 45  | 0: MESSAGE LINE|
00000120  00 23 43 02 00 3a 93 fb  20 20 20 20 20 36 34 3a  |.#C..:..     64:|
00000130  20 44 49 56 49 44 45 52  20 4c 49 4e 45 00 40 43  | DIVIDER LINE.@C|
00000140  03 00 3a 93 fb 31 32 38  2d 38 33 32 3a 20 4f 55  |..:..128-832: OU|
00000150  54 50 55 54 20 4c 49 4e  45 53 00 5d 43 04 00 3a  |TPUT LINES.]C..:|
00000160  93 fb 20 20 20 20 38 39  36 3a 20 44 49 56 49 44  |..    896: DIVID|
00000170  45 52 20 4c 49 4e 45 00  78 43 05 00 3a 93 fb 20  |ER LINE.xC..:.. |
00000180  20 20 20 39 36 30 3a 20  49 4e 50 55 54 20 4c 49  |   960: INPUT LI|
00000190  4e 45 00 84 43 0a 00 b8  28 31 30 30 30 29 00 94  |NE..C...(1000)..|
000001a0  43 14 00 54 d5 39 36 30  3a 4f d5 31 32 38 00 e7  |C..T.960:O.128..|
000001b0  43 19 00 59 24 d5 f7 28  33 33 29 cd f7 28 31 39  |C..Y$..(33)..(19|
000001c0  32 29 cd f7 28 36 30 29  cd f7 28 31 37 29 cd f7  |2)..(60)..(17)..|
000001d0  28 31 32 38 29 cd f7 28  36 30 29 cd f7 28 31 29  |(128)..(60)..(1)|
000001e0  cd f7 28 31 39 32 29 cd  f7 28 32 29 cd f7 28 32  |..(192)..(2)..(2|
000001f0  33 37 29 cd f7 28 31 37  36 29 cd f7 28 32 30 31  |37)..(176)..(201|
00000200  29 00 ed 43 1e 00 84 00  16 44 28 00 b2 20 40 20  |)..C.....D(.. @ |
00000210  36 34 2c c4 28 36 34 2c  31 34 30 29 3b 3a b2 20  |64,.(64,140);:. |
00000220  40 20 38 39 36 2c c4 28  36 34 2c 31 34 30 29 3b  |@ 896,.(64,140);|
00000230  00 44 44 32 00 41 24 d5  22 22 3a 42 24 d5 22 22  |.DD2.A$."":B$.""|
00000240  3a 43 24 d5 22 22 3a 44  24 d5 22 22 3a 45 24 d5  |:C$."":D$."":E$.|
00000250  22 22 3a 5a 24 d5 c4 28  36 33 2c 33 32 29 00 55  |"":Z$..(63,32).U|
00000260  44 64 00 81 20 49 d5 31  20 bd 20 31 30 30 30 00  |Dd.. I.1 . 1000.|
00000270  5e 44 6e 00 41 24 d5 c9  00 6e 44 78 00 8f 20 41  |^Dn.A$...nDx.. A|
00000280  24 d5 22 22 20 32 30 30  00 98 44 82 00 8f 20 41  |$."" 200..D... A|
00000290  24 d5 f7 28 38 29 20 91  20 32 30 30 30 3a 20 8d  |$..(8) . 2000: .|
000002a0  20 32 30 30 20 3a 93 fb  20 42 41 43 4b 53 50 41  | 200 :.. BACKSPA|
000002b0  43 45 00 c4 44 8c 00 8f  20 41 24 d5 f7 28 31 33  |CE..D... A$..(13|
000002c0  29 20 91 20 33 30 30 30  3a 20 8d 20 32 30 30 20  |) . 3000: . 200 |
000002d0  3a 93 fb 20 68 69 74 20  72 65 74 75 72 6e 00 d8  |:.. hit return..|
000002e0  44 96 00 8f 20 41 24 d5  f7 28 33 31 29 20 8d 20  |D... A$..(31) . |
000002f0  32 30 00 04 45 a0 00 8f  20 f6 28 41 24 29 d6 33  |20..E... .(A$).3|
00000300  32 20 d3 20 f6 28 41 24  29 d4 31 32 37 20 8d 20  |2 . .(A$).127 . |
00000310  32 30 30 20 3a 93 fb 20  49 47 4e 4f 52 45 00 20  |200 :.. IGNORE. |
00000320  45 aa 00 91 20 34 30 30  30 20 3a 93 fb 20 54 59  |E... 4000 :.. TY|
00000330  50 45 44 20 41 20 43 48  41 52 00 4e 45 c8 00 8f  |PED A CHAR.NE...|
00000340  20 d8 28 49 d0 32 30 29  20 d2 20 31 20 b2 20 40  | .(I.20) . 1 . @|
00000350  20 54 2c f7 28 31 34 33  29 3b 20 3a 95 20 b2 20  | T,.(143); :. . |
00000360  40 20 54 2c 22 20 22 3b  00 54 45 d2 00 87 00 5f  |@ T," ";.TE...._|
00000370  45 2c 01 91 20 38 30 30  30 00 69 45 e7 03 8d 20  |E,.. 8000.iE... |
00000380  31 30 30 00 80 45 e8 03  3a 93 fb 20 4d 45 53 53  |100..E..:.. MESS|
00000390  41 47 45 20 41 54 20 54  4f 50 00 8e 45 f2 03 b2  |AGE AT TOP..E...|
000003a0  20 40 20 30 2c 5a 24 3b  00 9c 45 fc 03 b2 20 40  | @ 0,Z$;..E... @|
000003b0  20 30 2c 43 24 3b 00 a2  45 cf 07 92 00 b4 45 d0  | 0,C$;..E.....E.|
000003c0  07 3a 93 fb 20 42 41 43  4b 53 50 41 43 45 00 c9  |.:.. BACKSPACE..|
000003d0  45 da 07 8f 20 f3 28 42  24 29 d5 30 20 8d 20 32  |E... .(B$).0 . 2|
000003e0  39 39 39 00 d8 45 e4 07  b2 20 40 20 54 2c 22 20  |999..E... @ T," |
000003f0  22 3b 00 e2 45 ee 07 54  d5 54 ce 31 00 f7 45 f8  |";..E..T.T.1..E.|
00000400  07 42 24 d5 f8 28 42 24  2c f3 28 42 24 29 ce 31  |.B$..(B$,.(B$).1|
00000410  29 00 fd 45 b7 0b 92 00  0a 46 b8 0b 3a 93 fb 20  |)..E.....F..:.. |
00000420  53 45 4e 44 00 4b 46 c2  0b 8f 20 f3 28 42 24 29  |SEND.KF... .(B$)|
00000430  d5 30 20 43 24 d5 22 2a  2a 20 54 59 50 45 20 41  |.0 C$."** TYPE A|
00000440  20 4d 53 47 20 42 45 46  4f 52 45 20 53 45 4e 44  | MSG BEFORE SEND|
00000450  49 4e 47 20 2a 2a 22 3a  91 20 31 30 30 30 3a 8d  |ING **":. 1000:.|
00000460  20 33 39 39 39 00 5b 46  cc 0b b2 20 40 20 39 36  | 3999.[F... @ 96|
00000470  30 2c 5a 24 3b 00 6c 46  d6 0b 44 24 d5 42 24 3a  |0,Z$;.lF..D$.B$:|
00000480  91 20 36 30 30 30 00 7a  46 e0 0b 45 24 d5 22 2a  |. 6000.zF..E$."*|
00000490  22 cd 42 24 00 85 46 ea  0b 91 20 39 30 30 30 00  |".B$..F... 9000.|
000004a0  8f 46 f4 0b 42 24 d5 22  22 00 9a 46 fe 0b 91 20  |.F..B$.""..F... |
000004b0  37 30 30 30 00 a4 46 08  0c 54 d5 39 36 30 00 aa  |7000..F..T.960..|
000004c0  46 9f 0f 92 00 bf 46 a0  0f 3a 93 fb 20 54 59 50  |F.....F..:.. TYP|
000004d0  45 44 20 41 20 43 48 41  52 00 fd 46 aa 0f 8f 20  |ED A CHAR..F... |
000004e0  f3 28 42 24 29 d4 d5 36  32 20 43 24 d5 22 2a 2a  |.(B$)..62 C$."**|
000004f0  20 4d 41 58 20 4d 53 47  20 4c 45 4e 47 54 48 20  | MAX MSG LENGTH |
00000500  49 53 20 36 32 20 2a 2a  22 3a 91 20 31 30 30 30  |IS 62 **":. 1000|
00000510  3a 8d 20 34 39 39 39 00  0b 47 b4 0f b2 20 40 20  |:. 4999..G... @ |
00000520  54 2c 41 24 3b 00 18 47  be 0f 42 24 d5 42 24 cd  |T,A$;..G..B$.B$.|
00000530  41 24 00 22 47 c8 0f 54  d5 54 cd 31 00 28 47 87  |A$."G..T.T.1.(G.|
00000540  13 92 00 41 47 70 17 3a  93 fb 20 53 45 4e 44 20  |...AGp.:.. SEND |
00000550  54 4f 20 43 41 53 53 45  54 54 45 00 67 47 7a 17  |TO CASSETTE.gGz.|
00000560  43 24 d5 22 2a 2a 20 53  45 4e 44 49 4e 47 20 4d  |C$."** SENDING M|
00000570  45 53 53 41 47 45 20 2a  2a 22 3a 91 20 31 30 30  |ESSAGE **":. 100|
00000580  30 00 74 47 84 17 b2 20  23 ce 31 2c 44 24 00 97  |0.tG... #.1,D$..|
00000590  47 8e 17 43 24 d5 22 2a  2a 20 4d 45 53 53 41 47  |G..C$."** MESSAG|
000005a0  45 20 53 45 4e 54 20 2a  2a 22 3a 91 20 31 30 30  |E SENT **":. 100|
000005b0  30 00 9d 47 57 1b 92 00  bb 47 58 1b 3a 93 fb 20  |0..GW....GX.:.. |
000005c0  52 45 43 45 49 56 45 20  46 52 4f 4d 20 43 41 53  |RECEIVE FROM CAS|
000005d0  53 45 54 54 45 00 e3 47  62 1b 43 24 d5 22 2a 2a  |SETTE..Gb.C$."**|
000005e0  20 52 45 43 45 49 56 49  4e 47 20 4d 45 53 53 41  | RECEIVING MESSA|
000005f0  47 45 20 2a 2a 22 3a 91  20 31 30 30 30 00 f0 47  |GE **":. 1000..G|
00000600  6c 1b 89 20 23 ce 31 2c  44 24 00 2b 48 76 1b 8f  |l.. #.1,D$.+Hv..|
00000610  20 f3 28 44 24 29 d5 30  20 43 24 d5 22 2a 2a 20  | .(D$).0 C$."** |
00000620  4e 4f 20 4d 45 53 53 41  47 45 20 52 45 43 45 49  |NO MESSAGE RECEI|
00000630  56 45 44 20 2a 2a 22 3a  91 20 31 30 30 30 3a 8d  |VED **":. 1000:.|
00000640  20 37 39 39 39 00 52 48  80 1b 43 24 d5 22 2a 2a  | 7999.RH..C$."**|
00000650  20 4d 45 53 53 41 47 45  20 52 45 43 45 49 56 45  | MESSAGE RECEIVE|
00000660  44 20 2a 2a 22 3a 91 20  31 30 30 30 00 97 48 8a  |D **":. 1000..H.|
00000670  1b 8f 20 44 24 d5 22 21  21 48 45 41 52 54 42 45  |.. D$."!!HEARTBE|
00000680  41 54 21 21 22 20 43 24  d5 22 2a 2a 20 48 45 41  |AT!!" C$."** HEA|
00000690  52 54 42 45 41 54 20 52  45 43 45 49 56 45 44 20  |RTBEAT RECEIVED |
000006a0  2a 2a 22 3a 91 20 31 30  30 30 3a 8d 20 37 39 39  |**":. 1000:. 799|
000006b0  39 00 b2 48 94 1b 8f 20  f3 28 44 24 29 d4 36 33  |9..H... .(D$).63|
000006c0  20 44 24 d5 f8 28 44 24  2c 36 33 29 00 c7 48 9e  | D$..(D$,63)..H.|
000006d0  1b 45 24 d5 f9 28 44 24  2c f3 28 44 24 29 ce 31  |.E$..(D$,.(D$).1|
000006e0  29 00 d2 48 a8 1b 91 20  39 30 30 30 00 eb 48 b2  |)..H... 9000..H.|
000006f0  1b 8f 20 f8 28 44 24 2c  31 29 d5 22 31 22 20 91  |.. .(D$,1)."1" .|
00000700  20 38 30 30 30 00 f1 48  3f 1f 92 00 03 49 40 1f  | 8000..H?....I@.|
00000710  3a 93 fb 20 48 45 41 52  54 42 45 41 54 00 2b 49  |:.. HEARTBEAT.+I|
00000720  4a 1f 43 24 d5 22 2a 2a  20 53 45 4e 44 49 4e 47  |J.C$."** SENDING|
00000730  20 48 45 41 52 54 42 45  41 54 20 2a 2a 22 3a 91  | HEARTBEAT **":.|
00000740  20 31 30 30 30 00 49 49  54 1f 44 24 d5 22 21 21  | 1000.IIT.D$."!!|
00000750  48 45 41 52 54 42 45 41  54 21 21 22 3a 91 20 36  |HEARTBEAT!!":. 6|
00000760  30 32 30 00 54 49 5e 1f  91 20 37 30 30 30 00 5a  |020.TI^.. 7000.Z|
00000770  49 27 23 92 00 7a 49 28  23 3a 93 fb 20 44 49 53  |I'#..zI(#:.. DIS|
00000780  50 4c 41 59 20 52 45 43  45 49 56 45 44 20 53 54  |PLAY RECEIVED ST|
00000790  52 49 4e 47 00 94 49 32  23 8f 20 4f d4 38 33 32  |RING..I2#. O.832|
000007a0  20 4f d5 38 33 32 3a 91  20 31 30 30 30 30 00 a2  | O.832:. 10000..|
000007b0  49 3c 23 b2 20 40 20 4f  2c 5a 24 3b 00 b0 49 46  |I<#. @ O,Z$;..IF|
000007c0  23 b2 20 40 20 4f 2c 45  24 3b 00 bb 49 50 23 4f  |#. @ O,E$;..IP#O|
000007d0  d5 4f cd 36 34 00 c1 49  0f 27 92 00 d8 49 10 27  |.O.64..I.'...I.'|
000007e0  3a 93 fb 20 53 43 52 4f  4c 4c 20 44 49 53 50 4c  |:.. SCROLL DISPL|
000007f0  41 59 00 ef 49 1a 27 b1  20 31 36 35 32 36 2c e5  |AY..I.'. 16526,.|
00000800  28 c0 28 59 24 29 cd 31  29 00 06 4a 24 27 b1 20  |(.(Y$).1)..J$'. |
00000810  31 36 35 32 37 2c e5 28  c0 28 59 24 29 cd 32 29  |16527,.(.(Y$).2)|
00000820  00 11 4a 2e 27 58 d5 c1  28 30 29 00 17 4a f7 2a  |..J.'X..(0)..J.*|
00000830  92 00 00 00                                       |....|
00000834

 */

int cassette_system(int fd)
{
   /*
    * This is exactly how the basic program would be stored in memory, starting at 42E9 (17129)
    *
    * Format of a given line of BASIC:
    *
    * (2) LSB MSB - address of the next line
    * (2) LSB MSB - line number
    * (n)         - bytes for the line.  BASIC keywords replaced with code.  Everything else in ascii
    * (1) 00
    *
    * Then at end:
    * (2) 00 00
    *
    *
    *
    * The dump below is an exact CSAVE of the above listed source.
    *
    */

   char *BASIC =

"06 43 01 00 3a 93 fb 20 20 20 20 20"
"20 30 3a 20 4d 45 53 53 41 47 45 20 4c 49 4e 45"
"00 23 43 02 00 3a 93 fb 20 20 20 20 20 36 34 3a"
"20 44 49 56 49 44 45 52 20 4c 49 4e 45 00 40 43"
"03 00 3a 93 fb 31 32 38 2d 38 33 32 3a 20 4f 55"
"54 50 55 54 20 4c 49 4e 45 53 00 5d 43 04 00 3a"
"93 fb 20 20 20 20 38 39 36 3a 20 44 49 56 49 44"
"45 52 20 4c 49 4e 45 00 78 43 05 00 3a 93 fb 20"
"20 20 20 39 36 30 3a 20 49 4e 50 55 54 20 4c 49"
"4e 45 00 84 43 0a 00 b8 28 31 30 30 30 29 00 94"
"43 14 00 54 d5 39 36 30 3a 4f d5 31 32 38 00 e7"
"43 19 00 59 24 d5 f7 28 33 33 29 cd f7 28 31 39"
"32 29 cd f7 28 36 30 29 cd f7 28 31 37 29 cd f7"
"28 31 32 38 29 cd f7 28 36 30 29 cd f7 28 31 29"
"cd f7 28 31 39 32 29 cd f7 28 32 29 cd f7 28 32"
"33 37 29 cd f7 28 31 37 36 29 cd f7 28 32 30 31"
"29 00 ed 43 1e 00 84 00 16 44 28 00 b2 20 40 20"
"36 34 2c c4 28 36 34 2c 31 34 30 29 3b 3a b2 20"
"40 20 38 39 36 2c c4 28 36 34 2c 31 34 30 29 3b"
"00 44 44 32 00 41 24 d5 22 22 3a 42 24 d5 22 22"
"3a 43 24 d5 22 22 3a 44 24 d5 22 22 3a 45 24 d5"
"22 22 3a 5a 24 d5 c4 28 36 33 2c 33 32 29 00 55"
"44 64 00 81 20 49 d5 31 20 bd 20 31 30 30 30 00"
"5e 44 6e 00 41 24 d5 c9 00 6e 44 78 00 8f 20 41"
"24 d5 22 22 20 32 30 30 00 98 44 82 00 8f 20 41"
"24 d5 f7 28 38 29 20 91 20 32 30 30 30 3a 20 8d"
"20 32 30 30 20 3a 93 fb 20 42 41 43 4b 53 50 41"
"43 45 00 c4 44 8c 00 8f 20 41 24 d5 f7 28 31 33"
"29 20 91 20 33 30 30 30 3a 20 8d 20 32 30 30 20"
"3a 93 fb 20 68 69 74 20 72 65 74 75 72 6e 00 d8"
"44 96 00 8f 20 41 24 d5 f7 28 33 31 29 20 8d 20"
"32 30 00 04 45 a0 00 8f 20 f6 28 41 24 29 d6 33"
"32 20 d3 20 f6 28 41 24 29 d4 31 32 37 20 8d 20"
"32 30 30 20 3a 93 fb 20 49 47 4e 4f 52 45 00 20"
"45 aa 00 91 20 34 30 30 30 20 3a 93 fb 20 54 59"
"50 45 44 20 41 20 43 48 41 52 00 4e 45 c8 00 8f"
"20 d8 28 49 d0 32 30 29 20 d2 20 31 20 b2 20 40"
"20 54 2c f7 28 31 34 33 29 3b 20 3a 95 20 b2 20"
"40 20 54 2c 22 20 22 3b 00 54 45 d2 00 87 00 5f"
"45 2c 01 91 20 38 30 30 30 00 69 45 e7 03 8d 20"
"31 30 30 00 80 45 e8 03 3a 93 fb 20 4d 45 53 53"
"41 47 45 20 41 54 20 54 4f 50 00 8e 45 f2 03 b2"
"20 40 20 30 2c 5a 24 3b 00 9c 45 fc 03 b2 20 40"
"20 30 2c 43 24 3b 00 a2 45 cf 07 92 00 b4 45 d0"
"07 3a 93 fb 20 42 41 43 4b 53 50 41 43 45 00 c9"
"45 da 07 8f 20 f3 28 42 24 29 d5 30 20 8d 20 32"
"39 39 39 00 d8 45 e4 07 b2 20 40 20 54 2c 22 20"
"22 3b 00 e2 45 ee 07 54 d5 54 ce 31 00 f7 45 f8"
"07 42 24 d5 f8 28 42 24 2c f3 28 42 24 29 ce 31"
"29 00 fd 45 b7 0b 92 00 0a 46 b8 0b 3a 93 fb 20"
"53 45 4e 44 00 4b 46 c2 0b 8f 20 f3 28 42 24 29"
"d5 30 20 43 24 d5 22 2a 2a 20 54 59 50 45 20 41"
"20 4d 53 47 20 42 45 46 4f 52 45 20 53 45 4e 44"
"49 4e 47 20 2a 2a 22 3a 91 20 31 30 30 30 3a 8d"
"20 33 39 39 39 00 5b 46 cc 0b b2 20 40 20 39 36"
"30 2c 5a 24 3b 00 6c 46 d6 0b 44 24 d5 42 24 3a"
"91 20 36 30 30 30 00 7a 46 e0 0b 45 24 d5 22 2a"
"22 cd 42 24 00 85 46 ea 0b 91 20 39 30 30 30 00"
"8f 46 f4 0b 42 24 d5 22 22 00 9a 46 fe 0b 91 20"
"37 30 30 30 00 a4 46 08 0c 54 d5 39 36 30 00 aa"
"46 9f 0f 92 00 bf 46 a0 0f 3a 93 fb 20 54 59 50"
"45 44 20 41 20 43 48 41 52 00 fd 46 aa 0f 8f 20"
"f3 28 42 24 29 d4 d5 36 32 20 43 24 d5 22 2a 2a"
"20 4d 41 58 20 4d 53 47 20 4c 45 4e 47 54 48 20"
"49 53 20 36 32 20 2a 2a 22 3a 91 20 31 30 30 30"
"3a 8d 20 34 39 39 39 00 0b 47 b4 0f b2 20 40 20"
"54 2c 41 24 3b 00 18 47 be 0f 42 24 d5 42 24 cd"
"41 24 00 22 47 c8 0f 54 d5 54 cd 31 00 28 47 87"
"13 92 00 41 47 70 17 3a 93 fb 20 53 45 4e 44 20"
"54 4f 20 43 41 53 53 45 54 54 45 00 67 47 7a 17"
"43 24 d5 22 2a 2a 20 53 45 4e 44 49 4e 47 20 4d"
"45 53 53 41 47 45 20 2a 2a 22 3a 91 20 31 30 30"
"30 00 74 47 84 17 b2 20 23 ce 31 2c 44 24 00 97"
"47 8e 17 43 24 d5 22 2a 2a 20 4d 45 53 53 41 47"
"45 20 53 45 4e 54 20 2a 2a 22 3a 91 20 31 30 30"
"30 00 9d 47 57 1b 92 00 bb 47 58 1b 3a 93 fb 20"
"52 45 43 45 49 56 45 20 46 52 4f 4d 20 43 41 53"
"53 45 54 54 45 00 e3 47 62 1b 43 24 d5 22 2a 2a"
"20 52 45 43 45 49 56 49 4e 47 20 4d 45 53 53 41"
"47 45 20 2a 2a 22 3a 91 20 31 30 30 30 00 f0 47"
"6c 1b 89 20 23 ce 31 2c 44 24 00 2b 48 76 1b 8f"
"20 f3 28 44 24 29 d5 30 20 43 24 d5 22 2a 2a 20"
"4e 4f 20 4d 45 53 53 41 47 45 20 52 45 43 45 49"
"56 45 44 20 2a 2a 22 3a 91 20 31 30 30 30 3a 8d"
"20 37 39 39 39 00 52 48 80 1b 43 24 d5 22 2a 2a"
"20 4d 45 53 53 41 47 45 20 52 45 43 45 49 56 45"
"44 20 2a 2a 22 3a 91 20 31 30 30 30 00 97 48 8a"
"1b 8f 20 44 24 d5 22 21 21 48 45 41 52 54 42 45"
"41 54 21 21 22 20 43 24 d5 22 2a 2a 20 48 45 41"
"52 54 42 45 41 54 20 52 45 43 45 49 56 45 44 20"
"2a 2a 22 3a 91 20 31 30 30 30 3a 8d 20 37 39 39"
"39 00 b2 48 94 1b 8f 20 f3 28 44 24 29 d4 36 33"
"20 44 24 d5 f8 28 44 24 2c 36 33 29 00 c7 48 9e"
"1b 45 24 d5 f9 28 44 24 2c f3 28 44 24 29 ce 31"
"29 00 d2 48 a8 1b 91 20 39 30 30 30 00 eb 48 b2"
"1b 8f 20 f8 28 44 24 2c 31 29 d5 22 31 22 20 91"
"20 38 30 30 30 00 f1 48 3f 1f 92 00 03 49 40 1f"
"3a 93 fb 20 48 45 41 52 54 42 45 41 54 00 2b 49"
"4a 1f 43 24 d5 22 2a 2a 20 53 45 4e 44 49 4e 47"
"20 48 45 41 52 54 42 45 41 54 20 2a 2a 22 3a 91"
"20 31 30 30 30 00 49 49 54 1f 44 24 d5 22 21 21"
"48 45 41 52 54 42 45 41 54 21 21 22 3a 91 20 36"
"30 32 30 00 54 49 5e 1f 91 20 37 30 30 30 00 5a"
"49 27 23 92 00 7a 49 28 23 3a 93 fb 20 44 49 53"
"50 4c 41 59 20 52 45 43 45 49 56 45 44 20 53 54"
"52 49 4e 47 00 94 49 32 23 8f 20 4f d4 38 33 32"
"20 4f d5 38 33 32 3a 91 20 31 30 30 30 30 00 a2"
"49 3c 23 b2 20 40 20 4f 2c 5a 24 3b 00 b0 49 46"
"23 b2 20 40 20 4f 2c 45 24 3b 00 bb 49 50 23 4f"
"d5 4f cd 36 34 00 c1 49 0f 27 92 00 d8 49 10 27"
"3a 93 fb 20 53 43 52 4f 4c 4c 20 44 49 53 50 4c"
"41 59 00 ef 49 1a 27 b1 20 31 36 35 32 36 2c e5"
"28 c0 28 59 24 29 cd 31 29 00 06 4a 24 27 b1 20"
"31 36 35 32 37 2c e5 28 c0 28 59 24 29 cd 32 29"
"00 11 4a 2e 27 58 d5 c1 28 30 29 00 17 4a f7 2a"
"92 00 00 00";

   /* Generate machine language style, loading the BASIC program to 42E9
    * Note we will also need to set the value in 40F9 to the address after the final 00 00.
    * That is a special 2 byte data block at the end.
    */

   unsigned char x;
   char basic[10000];
   char buf[20000];
   char *p,*q;
   int i;
   int load_address = 17129; /* 42E9 */
   int checksum;


   memset(buf,'\0',sizeof(buf));
   memset(basic,'\0',sizeof(basic));

   /* Pull the BASIC code into basic buffer, stripping non hex */
   for (p=BASIC,q=basic+strlen(basic); *p; p++)
   {
      if ( (*p != '\n') && (*p != ' ') )
      {
         *q = *p;
         q++;
      }

      if (q > basic+sizeof(basic))
      {
         perror("basic buffer too small for code");
         return(-1);
      }
   }

   /* leader and sync
    * 00 (255 of them)
    * A5
    */
   for (i=0; i<LEADER_LENGTH; i++)
   {
      sprintf(buf+strlen(buf),"%02x",LEADER_BYTE);
   }
   sprintf(buf+strlen(buf),"%02x",SYNC_BYTE);


   /* Filename header
    * 55 43 53 32 32 32 32  (CS    )
    */
   strcat(buf, "55435332323232");


   /* Data blocks of size <= fe (252) */
   p = basic;
   while (*p)
   {
      strcat(buf, "3c");

      i = DATA_BLOCK_MAX;

      i = (basic+strlen(basic)-p)/2;
      if (i > DATA_BLOCK_MAX) {i = DATA_BLOCK_MAX;}

      sprintf(buf+strlen(buf),"%02x",i);

      sprintf(buf+strlen(buf),"%02x%02x",(load_address%256),(int)(load_address/256));

      checksum = load_address%256;
      checksum = (checksum + (int)(load_address/256))%256;

      load_address += i;

      q = buf+strlen(buf);
      while (i>0)
      {
         *q = *p;
         *(q+1) = *(p+1);
         x=(((*p&0x40)?9+(*p&0x07):(*p&0x0f))<<4)|((*(p+1)&0x40)?9+(*(p+1)&0x07):(*(p+1)&0x0f));
         checksum = (checksum + x)%256;
         i--;
         p+=2;
         q+=2;
      }
      sprintf(buf+strlen(buf),"%02x",checksum);
   }


   /* One more data block
    * Need to set 40F9 to address after the program.  load_address will be pointing there already from last loop.
    * 2 byte block
    */
   strcat(buf, "3c02f940");
   sprintf(buf+strlen(buf),"%02x%02x",(load_address%256),(int)(load_address/256));
   checksum = 249;  /* f9 */
   checksum = (checksum + 64)%256;  /* 40 */
   checksum = (checksum + (load_address%256))%256;
   checksum = (checksum + (int)(load_address/256))%256;
   sprintf(buf+strlen(buf),"%02x",checksum);

   /* entry header */
   strcat(buf, "78e81a");  /* begin execution at the end of new (last) input line.  i.e. prompt */

   /* extra crap on the end to flush the descriptor out */
   strcat(buf, "00000000000000000000");

   printf("system file:\n%s\n", buf);

   return(write_hex_string(fd, buf, 0));
}
