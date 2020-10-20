/*
 * 1. On TRS-80
 *
 *    >SYSTEM
 *    *? KP
 *
 * 2. Run this program on laptop, which will run cassette_system() to transfer
 *    over the machine code to TRS-80 and then fall into a loop:
 *
 *    - Prompt "Krabby Patty code?: "
 *    - Read in an int
 *    - Send "leader and sync byte" (255 0s followed by A5)
 *    - Send 4 bytes representing the int in little endian order
 *
 * 3. On TRS-80
 *
 *    *? /
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
void append(char *buf, unsigned char c);
int cassette_system(int fd, char *pgm, int load_address, int entry_address, char *code);
int leader_and_sync(int fd);
int write_string(int fd, char *s);
int send_int(int fd, int i);
int write_hex_string(int fd, char *s);
int write_byte(int fd, unsigned char c);
void flush(int fd);
char* parse_machine_code(char *in);


#define SOUND_PCM_WRITE_BITS ( 1610895365 )
#define SOUND_PCM_WRITE_CHANNELS ( 1610895366 )
#define SOUND_PCM_WRITE_RATE ( 1610895362 )
#define SOUND_PCM_SYNC ( 20481 )

#define RATE 11025
#define SIZE 8      /* sample size: 8 or 16 bits */
#define CHANNELS 1  /* 1 = mono 2 = stereo */

#define LEADER_BYTE ( 0x00 )
#define LEADER_LENGTH 255
#define SYNC_BYTE   ( 0xa5 )
#define DATA_BLOCK_MAX ( 256 )

#define END_STRING_BYTE_LENGTH ( 10 )
#define END_STRING_BYTE ( 0x0d )

#define FILENAME_HEADER ( 0x55 )
#define DATA_HEADER ( 0x3c )
#define ENTRY_HEADER ( 0x78 )
#define PROGRAM_NAME ( "KP" )
#define LOAD_ADDRESS ( 0x7000 )
#define BASIC_ENTRY ( 0x06cc )

#define APPEND(x) (append(buf,(x)))

typedef int bool;
#define TRUE ( 1 )
#define FALSE ( 0 )

struct machine_code {
   int  load_address;
   int  entry_address;
   bool parse;
   char *code;
};

typedef struct machine_code MACHINE_CODE;




/*
 * Hand assembled machine code
 *
 * For simplicity, the code is listed in assembler with hand assembled
 * machine code.  The machine code bytes are parsed from the "code".
 *
 * Assumption for this formatting:
 *
 * 1. Any lines with machine code will start with a digit.
 * 2. The machine code is assummed to be in field 2, using
 *    TAB as the delimeter.
 * 3. No other string manipulation is done.
 *
 * Set the "parse" value to FALSE to skip this parsing.
 */

MACHINE_CODE code_examples[] = {

   /*
    * Raw data test 0 - 255
    */
   {
      load_address:  0x7000,
      entry_address: BASIC_ENTRY,
      parse: FALSE,
      code:
"00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f"
"10 11 12 13 14 15 16 17 18 19 1a 1b 1c 1d 1e 1f"
"20 21 22 23 24 25 26 27 28 29 2a 2b 2c 2d 2e 2f"
"30 31 32 33 34 35 36 37 38 39 3a 3b 3c 3d 3e 3f"
"40 41 42 43 44 45 46 47 48 49 4a 4b 4c 4d 4e 4f"
"50 51 52 53 54 55 56 57 58 59 5a 5b 5c 5d 5e 5f"
"60 61 62 63 64 65 66 67 68 69 6a 6b 6c 6d 6e 6f"
"70 71 72 73 74 75 76 77 78 79 7a 7b 7c 7d 7e 7f"
"80 81 82 83 84 85 86 87 88 89 8a 8b 8c 8d 8e 8f"
"90 91 92 93 94 95 96 97 98 99 9a 9b 9c 9d 9e 9f"
"a0 a1 a2 a3 a4 a5 a6 a7 a8 a9 aa ab ac ad ae af"
"b0 b1 b2 b3 b4 b5 b6 b7 b8 b9 ba bb bc bd be bf"
"c0 c1 c2 c3 c4 c5 c6 c7 c8 c9 ca cb cc cd ce cf"
"d0 d1 d2 d3 d4 d5 d6 d7 d8 d9 da db dc dd de df"
"e0 e1 e2 e3 e4 e5 e6 e7 e8 e9 ea eb ec ed ee ef"
"f0 f1 f2 f3 f4 f5 f6 f7 f8 f9 fa fb fc fd fe ff"
   },
   {
      load_address:  0x7000,
      entry_address: BASIC_ENTRY,
      parse: TRUE,
      code:
"										\n"
"; Simple test program								\n"
";										\n"
"; 1. Clear the screen								\n"
"; 2. Jump to BASIC								\n"
";										\n"
"			ORG	7000H						\n"
"7000	CD C9 01	CALL	01C9H	; Clear screen				\n"
"7003	C3 38 1A	JP	1A38H	; Jump to BASIC				\n"
   },
   {
      load_address:  0x7000,
      entry_address: 0x7000,
      parse: TRUE,
      code:
"										\n"
"; Print HELLO, WORLD!								\n"
"										\n"
"				ORG	7000H					\n"
"7000	CD C9 01		CALL	01C9H	; Clear screen			\n"
"7003	21 15 70		LD	HL, STR					\n"
"7006	7E		LOOP	LD	A, (HL)					\n"
"7007	B7			OR	A					\n"
"7008	28 08			JR	Z, DONE					\n"
"700A	E5			PUSH	HL					\n"
"700B	CD 33 00		CALL	0033H	; Print char in A		\n"
"700E	E1			POP	HL					\n"
"700F	23			INC	HL					\n"
"7010	18 F4			JR	LOOP					\n"
"7012	C3 CC 06	DONE	JP	06CCH	; Jump to BASIC			\n"
"7015			STR	DEFM	'HELLO, WORLD!'				\n"
"				DEFB	0DH					\n"
"				DEFB	0					\n"
"										\n"
"; Data (listing the actual bytes)						\n"
"7015	48 45 4C 4C 4F 2C 20 57 4F 52 4C 44 21 0D 00				\n"
   },
   {
      load_address:  0x7000,
      entry_address: 0x7000,
      parse: TRUE,
      code:
"; Print 'KRABBY PATTY CODE?: ' using routine 'PRINTS'				\n"
"										\n"
"				ORG	7000H					\n"
"7000	CD C9 01		CALL	01C9H	; Clear screen			\n"
"7003	21 15 70		LD	HL, STR					\n"
"7006	CD 0C 70		CALL	PRINTS					\n"
"7009	C3 CC 06	    	JP	06CCH	; Jump to BASIC			\n"
"										\n"
"700C	7E		PRINTS	LD	A, (HL)	; Prints the string pointed to	\n"
"700D	B7			OR	A	; by HL.  Null terminated.	\n"
"700E	C8			RET	Z					\n"
"700F	CD 33 00		CALL	0033H	; Print char in A		\n"
"7012	23			INC	HL					\n"
"7013	18 F7			JR	PRINTS					\n"
"										\n"
"7015			STR	DEFM	'KRABBY PATTY CODE?: '			\n"
"				DEFB	0					\n"
"										\n"
"; Data (listing the actual bytes)						\n"
"7015	4B 52 41 42 42 59 20 50 41 54 54 59 20 43 4F 44 45 3F 20		\n"
   },
   {
      load_address:  0x7000,
      entry_address: 0x7000,
      parse: TRUE,
      code:
"; Loop:											\n"
";   - read a byte from cassette								\n"
";   - print hex code for byte followed by a carriage return					\n"
"				ORG	7000H							\n"
"												\n"
"7000	CD C9 01		CALL	01C9H		; ROM - Clear screen and home cursor	\n"
"7003	CD 96 02	READB 	CALL	0296H		; ROM - Read leader and sync byte from cassette	\n"
"7006	CD 35 02	     	CALL	0235H		; ROM - Read a byte into A from cassette	\n"
"7009	47			LD	B,A		; Save into B				\n"
"700A	0F			RRCA								\n"
"700B	0F			RRCA								\n"
"700C	0F			RRCA								\n"
"700D	0F			RRCA								\n"
"700E	E6 0F			AND	0FH							\n"
"7010	C6 30			ADD	A, 30H							\n"
"7012	FE 3A			CP	3AH							\n"
"7014	38 02			JR	C, DISP							\n"
"7016	C6 07			ADD	A, 07H							\n"
"7018	CD 33 00	DISP	CALL	0033H		; ROM - Print char in A			\n"
"701B	78			LD	A, B							\n"
"701C	E6 0F			AND	0FH							\n"
"701E	C6 30			ADD	A, 30H							\n"
"7020	FE 3A			CP	3AH							\n"
"7022	38 02			JR	C, DISP2						\n"
"7024	C6 07			ADD	A, 07H							\n"
"7026	CD 33 00	DISP2	CALL	0033H		; ROM - Print char in A			\n"
"7029	3E 0D			LD	A, 0DH		; Print a carriage return		\n"
"702B	CD 33 00	     	CALL	0033H		; ROM - Print char in A			\n"
"702E	18 D3			JR	READB							\n"
   },
   {
      load_address:  0x7000,
      entry_address: 0x7000,
      parse: TRUE,
      code:
"; Loop:											\n"
";   Print 'KRABBY PATTY CODE?: ' using routine 'PRINTS'					\n"
";   Read 4 bytes from cassette port								\n"
";   Print the 4 bytes in hex									\n"
";   Print a carriage return									\n"
";												\n"
"; Uses these Level II ROM routines								\n"
"; See http://www.trs-80.com/wordpress/zaps-patches-pokes-tips/rom-explained-part-1/		\n"
";												\n"
"; 01C9H - LEVEL II BASIC CLS ROUTINE								\n"
"; 0296H - Read leader and sync byte from cassette						\n"
"; 0235H - CASSETTE ROUTINE (READ A BYTE) 							\n"
"; 0033H - VIDEO ROUTINE - print the char in A to screen					\n"
"; 												\n"
"												\n"
"				ORG	7000H							\n"
"												\n"
"7000	CD C9 01		CALL	01C9H		; ROM - Clear screen and home cursor	\n"
"												\n"
"7003	21 65 70	MAIN	LD	HL, STR1						\n"
"7006	CD 58 70		CALL	PRINTS		; Print STR1				\n"
"												\n"
"7009	21 61 70		LD	HL, DATAB	; Read 4 bytes into memory		\n"
"700C	06 04			LD	B, 4							\n"
"700E	CD 1E 70		CALL	READBS							\n"
"												\n"
"7011	21 61 70		LD	HL, DATAB	; Print the 4 bytes read		\n"
"7014	06 04			LD	B, 4							\n"
"7016	CD 29 70		CALL	PRINTBS							\n"
"												\n"
"7019	CD 52 70		CALL	PRINTCR		; Print a carriage return		\n"
"												\n"
"701C	18 E5			JR	MAIN		; Do it again				\n"
"												\n"
"												\n"
"; READBS											\n"
"; Read bytes from cassette port into address in HL.						\n"
"; Reads B many bytes.										\n"
"; A, B,  and HL are modified by this routine							\n"
"701E	CD 96 02	READBS	CALL	0296H		; ROM - Read leader and sync byte from cassette	\n"
"7021	CD 35 02	READB	CALL	0235H		; ROM - Read a byte into A from cassette	\n"
"7024	77			LD	(HL), A		; Store A into memory			\n"
"7025	23			INC	HL							\n"
"7026	10 F9			DJNZ	READB							\n"
"7028	C9			RET								\n"
"												\n"
"												\n"
"; PRINTBS											\n"
"; Print the bytes pointed to by HL.								\n"
"; Prints B many bytes.										\n"
"; A, B,  and HL are modified by this routine							\n"
"7029	3E 20		PRINTBS	LD	A, ' '		; Print a space				\n"
"702B	CD 33 00		CALL	0033H		; ROM - Print char in A			\n"
"702E	7E			LD	A, (HL)		; Print high order nibble		\n"
"702F	0F			RRCA								\n"
"7030	0F			RRCA								\n"
"7031	0F			RRCA								\n"
"7032	0F			RRCA								\n"
"7033	E6 0F			AND	0FH							\n"
"7035	C6 30			ADD	A, 30H							\n"
"7037	FE 3A			CP	3AH							\n"
"7039	38 02			JR	C, DISP							\n"
"703B	C6 07			ADD	A, 07H							\n"
"703D	CD 33 00	DISP	CALL	0033H		; ROM - Print char in A			\n"
"7040	7E			LD	A, (HL)		; Print low order nibble		\n"
"7041	E6 0F			AND	0FH							\n"
"7043	C6 30			ADD	A, 30H							\n"
"7045	FE 3A			CP	3AH							\n"
"7047	38 02			JR	C, DISP2						\n"
"7049	C6 07			ADD	A, 07H							\n"
"704B	CD 33 00	DISP2	CALL	0033H		; ROM - Print char in A			\n"
"704E	23			INC	HL							\n"
"704F	10 D8			DJNZ	PRINTBS							\n"
"7051	C9			RET								\n"
"												\n"
"												\n"
"; PRINTCR											\n"
"; Prints a carriage return									\n"
"; A is modified by this routine								\n"
"7052	3E 0D		PRINTCR	LD	A, 0DH							\n"
"7054	CD 33 00		CALL	0033H		; ROM - Print char in A			\n"
"7057	C9			RET								\n"
"												\n"
"												\n"
"; PRINTS											\n"
"; Prints the NULL terminated string with address in HL						\n"
"; A and HL are modified by this routine							\n"
"7058	7E		PRINTS	LD	A, (HL)	; Prints the string pointed to by HL.  Null terminated	\n"
"7059	B7			OR	A							\n"
"705A	C8			RET	Z							\n"
"705B	CD 33 00		CALL	0033H	; ROM - Print char in A				\n"
"705E	23			INC	HL							\n"
"705F	18 F7			JR	PRINTS							\n"
"												\n"
"												\n"
"7061			DATAB	DEFS	4							\n"
"7065			STR1	DEFM	'KRABBY PATTY CODE?:'					\n"
"				DEFB	0							\n"
"; Data (listing the actual bytes)								\n"
"7061	00 00 00 00										\n"
"7065	4B 52 41 42 42 59 20 50 41 54 54 59 20 43 4F 44 45 3F 00				\n"
   },
   {
      load_address:  0x7000,
      entry_address: 0x7000,
      parse: TRUE,
      code:
"; Testing out the ROM routine to convert 4 bytes to an ascii string				\n"
"; 305419896 = 12345678H									\n"
"												\n"
"				ORG	7000H							\n"
"												\n"
"7000	CD C9 01		CALL	01C9H		; ROM - Clear screen and home cursor	\n"
"7003	21 21 41		LD	HL, 4121H						\n"
"7003	3E 12			LD	A, 12H							\n"
"7005	77			LD	(HL), A	 						\n"
"7006	23			INC	HL							\n"
"7007	3E 34			LD	A, 34H							\n"
"7009	77			LD	(HL), A	 						\n"
"700A	23			INC	HL							\n"
"700B	3E 56			LD	A, 56H							\n"
"700D	77			LD	(HL), A	 						\n"
"700E	23			INC	HL							\n"
"700F	3E 78			LD	A, 78H							\n"
"7011	77			LD	(HL), A	 						\n"
"7012	21 AF 40		LD	HL, 40AFH						\n"
"7015	3E 04			LD	A, 4							\n"
"7017	77			LD	(HL), A							\n"
"7018	CD BD 0F		CALL	0FBDH							\n"
"701B	CD A7 28		CALL	28A7H							\n"
"701E	C3 CC 06	    	JP	06CCH	; Jump to BASIC					\n"
   },
   {
      load_address:  0x7000,
      entry_address: 0x7000,
      parse: TRUE,
      code:
"; ROM routines used:										\n"
"; 01C9H - Clear the screen and home the cursor							\n"
"; 0296H - Read the leader (255 0s) and sync byte (a5) from cassette port			\n"
"; 0235H - Read a byte from cassette port into A						\n"
"; 28A7H - Display the NULL terminated string pointed to by HL					\n"
"				ORG	7000H							\n"


"7000	CD C9 01		CALL	01C9H		; ROM - Clear screen and home cursor	\n"


"; Read in 4 bytes										\n"
"7003	21 DA 70	MAIN	LD	HL, DATAB						\n"
"7006	06 04			LD	B, 4							\n"
"7008	CD 96 02		CALL	0296H		; ROM - Read leader and sync byte from cassette \n"
"700B	CD 35 02	READB	CALL	0235H		; ROM - Read a byte into A from cassette\n"
"700E	77			LD	(HL), A		; Save the byte to memory		\n"
"700F	23			INC	HL							\n"
"7010	10 F9			DJNZ	READB							\n"

"; Copy the 4 bytes to save area								\n"
"7012	21 DA 70		LD	HL, DATAB	; A bit overkill to just copy		\n"
"7015	11 DE 70		LD	DE, SAVEB	; 4 bytes, but trying out LDIR		\n"
"7018	01 04 00		LD	BC, 4							\n"
"701B	ED B0			LDIR								\n"


"; Print the bytes read										\n"

"701D	CD C9 01		CALL	01C9H		; ROM - Clear screen and home cursor	\n"
"7020	21 15 71		LD	HL, STR1	; '\rKrabby Patty code: '		\n"
"7023	CD A7 28		CALL	28A7H		; Print string pointed to by HL		\n"

"; IX - Number (DATAB)										\n"
"; IY = Powers of 10 (POW10)									\n"
"; HL = Result (DATAS)										\n"

"7026	21 E2 70		LD	HL, DATAS	; HL = Result				\n"
"7029	DD 21 DA 70		LD	IX, DATAB	; IX = Number				\n"
"702D	FD 21 ED 70		LD	IY, POW10	; IY = Powers of 10 table		\n"

"7031	06 0A			LD	B, 10		; 10 digits to compute			\n"
"7033	36 2F		LOOPDIG	LD	(HL), '/'	; one character below a 0		\n"

"7035	34		LOOPPOW	INC	(HL)		; Increment result char in place	\n"

"7036	37			SCF			; Clear the carry flag			\n"
"7037	3F			CCF								\n"

"7038	DD 7E 00		LD	A, (IX+0)	; Subtract the power of 10 from		\n"
"703B	FD 9E 00		SBC	A, (IY+0)	; the number.				\n"
"703E	DD 77 00		LD	(IX+0), A	;					\n"
"7041	DD 7E 01		LD	A, (IX+1)	; Flattened out for ease of coding.	\n"
"7044	FD 9E 01		SBC	A, (IY+1)	;					\n"
"7047	DD 77 01		LD	(IX+1), A	;					\n"
"704A	DD 7E 02		LD	A, (IX+2)	;					\n"
"704D	FD 9E 02		SBC	A, (IY+2)	;					\n"
"7050	DD 77 02		LD	(IX+2), A	;					\n"
"7053	DD 7E 03		LD	A, (IX+3)	;					\n"
"7056	FD 9E 03		SBC	A, (IY+3)	;					\n"
"7059	DD 77 03		LD	(IX+3), A	;					\n"

"705C	30 D7			JR	NC, LOOPPOW	;					\n"

"705E	37			SCF			; Clear the carry flag			\n"
"705F	3F			CCF								\n"

"7060	DD 7E 00		LD	A, (IX+0)	; Add back in the power of 10 once.	\n"
"7063	FD 8E 00		ADC	A, (IY+0)	;					\n"
"7066	DD 77 00		LD	(IX+0), A	; Flattened out for ease of coding.	\n"
"7069	DD 7E 01		LD	A, (IX+1)	;					\n"
"706C	FD 8E 01		ADC	A, (IY+1)	;					\n"
"706F	DD 77 01		LD	(IX+1), A	;					\n"
"7072	DD 7E 02		LD	A, (IX+2)	;					\n"
"7075	FD 8E 02		ADC	A, (IY+2)	;					\n"
"7078	DD 77 02		LD	(IX+2), A	;					\n"
"707B	DD 7E 03		LD	A, (IX+3)	;					\n"
"707E	FD 8E 03		ADC	A, (IY+3)	;					\n"
"7081	DD 77 03		LD	(IX+3), A	;					\n"

"7084	23			INC	HL		; Move on to next result digit.		\n"

"7085	FD 23			INC	IY		; Move on to next power of 10.		\n"
"7087	FD 23			INC	IY		;					\n"
"7089	FD 23			INC	IY		;					\n"
"708B	FD 23			INC	IY		;					\n"

"708D	10 A4			DJNZ	LOOPDIG		; B=B-1.  Loop for next result digit.	\n"

"708F	36 00			LD	(HL), 0		; NULL terminate the result		\n"
"7091	21 E2 70		LD	HL, DATAS	; HL = Result				\n"
"7094	CD A7 28		CALL	28A7H		; Print string pointed to by HL		\n"

"; Print the ingredients									\n"

"7097	21 2A 71		LD	HL, STR2	; '\rThat Krabby Patty needs...\r'	\n"
"709A	CD A7 28		CALL	28A7H		; Print string pointed to by HL		\n"

"; HL = Ingredients										\n"
"; DE = Saved bytes										\n"
";  B = bit counter										\n"

"709D	21 47 71		LD	HL, INGRED						\n"
"70A0	11 DE 70		LD	DE, SAVEB						\n"
"70A3	06 08			LD	B, 8							\n"
"70A5	3E 0D			LD	A, 0DH		; Set delimeter to '\r'			\n"
"70A7	32 46 71		LD	(DELIM), A						\n"

"70AA	3E FF		CHECKI	LD	A, FFH		; Check if done with ingredients	\n"
"70AC	BE			CP	(HL)							\n"
"70AD	CA 03 70		JP	Z, MAIN							\n"

"70B0	1A			LD	A, (DE)							\n"
"70B1	0F			RRCA			; Sets carry flag if bit 0 is set	\n"
"70B2	12			LD	(DE), A							\n"
"70B3	30 18			JR	NC, FINDI						\n"

"70B5	C5			PUSH	BC		; ROM routines clobber HL, DE, BC	\n"	
"70B6	D5			PUSH	DE							\n"
"70B7	E5			PUSH	HL		; Pushing DE and HL twice, for		\n"
"70B8	D5			PUSH	DE		; two ROM routines to be called.	\n"
"70B9	E5			PUSH	HL							\n"
"70BA	3A 46 71		LD	A, (DELIM)						\n"
"70BD	CD 33 00		CALL	0033H		; ROM - print character in A		\n"
"70C0	3E 2C			LD	A, 2CH		; Set delimeter to ','			\n"
"70C2	32 46 71		LD	(DELIM), A	; Save back in memory			\n"
"70C5	E1			POP	HL		; Restore HL and DE			\n"
"70C6	D1			POP	DE							\n"
"70C7	CD A7 28		CALL	28A7H		; ROM - print the ingredient		\n"
"70CA	E1			POP	HL		; Restore HL, DE, and BC		\n"
"70CB	D1			POP	DE							\n"
"70CC	C1			POP	BC							\n"

"70CD	AF		FINDI	XOR	A		; Search for next ingredient by		\n"
"70CE	23		LOOPI	INC	HL		; looking for NULL terminator.		\n"
"70CF	BE			CP	(HL)							\n"
"70D0	20 FC			JR	NZ, LOOPI						\n"
"70D2	23			INC	HL		; Point HL to next ingredient.		\n"

"70D3	10 D5			DJNZ	CHECKI							\n"

"70D5	13			INC	DE		; Need to move to next byte		\n"
"70D6	06 08			LD	B, 8							\n"
"70D8	18 D0			JR	CHECKI							\n"



"; Data												\n"

"70DA	00 00 00 00	DATAB	DEFB	4		; Memory for 4 read bytes		\n"

"70DE	00 00 00 00	SAVEB	DEFB	4		; Memory for saving the 4 bytes		\n"

"70E2			DATAS	DEFB	11		; Memory for 10 digits + NULL		\n"
"70E2	00 00 00 00 00 00 00 00 00 00 00							\n"

"70ED	00 CA 9A 3B	POW10	DEFB	40		; 1,000,000,000				\n"
"70F1	00 E1 F5 05					;   100,000,000				\n"
"70F5	80 96 98 00					;    10,000,000				\n"
"70F9	40 42 0F 00					;     1,000,000				\n"
"71FD	A0 86 01 00					;       100,000				\n"
"7101	10 27 00 00					;        10,000				\n"
"7105	E8 03 00 00					;         1,000				\n"
"7109	64 00 00 00					;           100				\n"
"710D	0A 00 00 00					;            10				\n"
"7111	01 00 00 00					;             1				\n"

"7115	0D		STR1	DEFB	0DH							\n"
"7116				DEFM	'Krabby Patty code: '					\n"
"7116	4B 72 61 62 62 79 20 50 61 74 74 79 20 63 6F 64 65 3A 20				\n"
"7129	00			DEFB	0							\n"

"712A	0D		STR2	DEFB	0DH							\n"
"712A				DEFM	'That Krabby Patty needs...'				\n"
"712B	54 68 61 74 20 4B 72 61 62 62 79 20 50 61 74 74 79 20 6E 65 65 64 73 2E 2E 2E		\n"
"7145	00			DEFB	0							\n"

"7146	0D		DELIM	DEB	0DH							\n"

"7147			INGRED	DEFB								\n"
"; 'Jamaican Jerk Mustard', 0		\n"
"; 'Mustard Steak Sauce', 0		\n"
"; 'Curry Ketchup', 0		\n"
"; 'Spicy Cocktail Sauce', 0		\n"
"; 'Pineapple Mayonnaise', 0		\n"
"; 'Spicy Lime Mayonnaise', 0		\n"
"; 'Ranch Mayonnaise', 0		\n"
"; 'Yogurt Feta Sauce', 0		\n"
"; 'Olive Relish', 0		\n"
"; 'Spicy Pepper Relish', 0		\n"
"; 'Grilled Kimchi Relish', 0		\n"
"; 'Middle Eastern Chickpea Relish', 0		\n"
"; 'Barbecue Sauce', 0		\n"
"; 'Coffee Barbecue Sauce', 0		\n"
"; 'Ginger-Hoisin Barbecue Sauce', 0		\n"
"; 'White Barbecue Sauce', 0		\n"
"; 'Mustard-Pepper Cream Sauce', 0		\n"
"; 'Curried Mango Sauce', 0		\n"
"; 'Gruyere Onions Saute', 0		\n"
"; 'Worcestershire Onions', 0		\n"
"; 'Cajun Onion Straws', 0		\n"
"; 'Garlic-Miso Sauce', 0		\n"
"; 'Bourbon Bacon Jam', 0		\n"
"; 'Honey Mustard-Glazed Bacon', 0		\n"
"; 'Sugared Pecan Bacon', 0		\n"
"; 'Lemon-Pepper Bacon', 0		\n"
"; 'Bacon Peanut Butter', 0		\n"
"; 'Spicy Blue Cheese Butter', 0		\n"
"; 'Buffalo Butter', 0		\n"
"; 'Peruvian Pepper Sauce', 0		\n"
"; 'Green Chile Sauce', 0		\n"
"; 'Chipotle Black Bean Sauce', 0		\n"
"714A	4A 61 6D 61 69 63 61 6E 20 4A 65 72 6B 20 4D 75 73 74 61 72 64 00		\n"
"7160	4D 75 73 74 61 72 64 20 53 74 65 61 6B 20 53 61 75 63 65 00		\n"
"7174	43 75 72 72 79 20 4B 65 74 63 68 75 70 00		\n"
"7182	53 70 69 63 79 20 43 6F 63 6B 74 61 69 6C 20 53 61 75 63 65 00		\n"
"7197	50 69 6E 65 61 70 70 6C 65 20 4D 61 79 6F 6E 6E 61 69 73 65 00		\n"
"71AC	53 70 69 63 79 20 4C 69 6D 65 20 4D 61 79 6F 6E 6E 61 69 73 65 00		\n"
"71C2	52 61 6E 63 68 20 4D 61 79 6F 6E 6E 61 69 73 65 00		\n"
"71D3	59 6F 67 75 72 74 20 46 65 74 61 20 53 61 75 63 65 00		\n"
"71E5	4F 6C 69 76 65 20 52 65 6C 69 73 68 00		\n"
"71F2	53 70 69 63 79 20 50 65 70 70 65 72 20 52 65 6C 69 73 68 00		\n"
"7206	47 72 69 6C 6C 65 64 20 4B 69 6D 63 68 69 20 52 65 6C 69 73 68 00		\n"
"721C	4D 69 64 64 6C 65 20 45 61 73 74 65 72 6E 20 43 68 69 63 6B 70 65 61 20 52 65 6C 69 73 68 00		\n"
"723B	42 61 72 62 65 63 75 65 20 53 61 75 63 65 00		\n"
"724A	43 6F 66 66 65 65 20 42 61 72 62 65 63 75 65 20 53 61 75 63 65 00		\n"
"7260	47 69 6E 67 65 72 2D 48 6F 69 73 69 6E 20 42 61 72 62 65 63 75 65 20 53 61 75 63 65 00		\n"
"727D	57 68 69 74 65 20 42 61 72 62 65 63 75 65 20 53 61 75 63 65 00		\n"
"7292	4D 75 73 74 61 72 64 2D 50 65 70 70 65 72 20 43 72 65 61 6D 20 53 61 75 63 65 00		\n"
"72AD	43 75 72 72 69 65 64 20 4D 61 6E 67 6F 20 53 61 75 63 65 00		\n"
"72C1	47 72 75 79 65 72 65 20 4F 6E 69 6F 6E 73 20 53 61 75 74 65 00		\n"
"72D6	57 6F 72 63 65 73 74 65 72 73 68 69 72 65 20 4F 6E 69 6F 6E 73 00		\n"
"72EC	43 61 6A 75 6E 20 4F 6E 69 6F 6E 20 53 74 72 61 77 73 00		\n"
"72FF	47 61 72 6C 69 63 2D 4D 69 73 6F 20 53 61 75 63 65 00		\n"
"7311	42 6F 75 72 62 6F 6E 20 42 61 63 6F 6E 20 4A 61 6D 00		\n"
"7323	48 6F 6E 65 79 20 4D 75 73 74 61 72 64 2D 47 6C 61 7A 65 64 20 42 61 63 6F 6E 00		\n"
"733E	53 75 67 61 72 65 64 20 50 65 63 61 6E 20 42 61 63 6F 6E 00		\n"
"7352	4C 65 6D 6F 6E 2D 50 65 70 70 65 72 20 42 61 63 6F 6E 00		\n"
"7365	42 61 63 6F 6E 20 50 65 61 6E 75 74 20 42 75 74 74 65 72 00		\n"
"7379	53 70 69 63 79 20 42 6C 75 65 20 43 68 65 65 73 65 20 42 75 74 74 65 72 00		\n"
"7392	42 75 66 66 61 6C 6F 20 42 75 74 74 65 72 00		\n"
"73A1	50 65 72 75 76 69 61 6E 20 50 65 70 70 65 72 20 53 61 75 63 65 00		\n"
"73B7	47 72 65 65 6E 20 43 68 69 6C 65 20 53 61 75 63 65 00		\n"
"73C9	43 68 69 70 6F 74 6C 65 20 42 6C 61 63 6B 20 42 65 61 6E 20 53 61 75 63 65 00		\n"
"73E3	FF		; End of ingredients		\n"
   }
};


int main(int argc, char *argv[])
{
  int fd;
  int inx;
  int i;

  if (argc==2)
  {
     inx = atoi(argv[1]);
     if ( (inx<0) || (inx>=sizeof(code_examples)/sizeof(code_examples[0])) )
     {
        inx = 0;
     }
  }
  else
  {
     inx = sizeof(code_examples)/sizeof(code_examples[0]) - 1;
  }


  if (initialize(&fd) < 0)
  {
     perror("Fail");
     exit(1);
  }

  /*
   * Need to send over the machine code first.  Using Machine Language
   * Object (SYSTEM) Tape format.
   */
  if (cassette_system(fd, PROGRAM_NAME, code_examples[inx].load_address, code_examples[inx].entry_address, code_examples[inx].parse ? parse_machine_code(code_examples[inx].code) : code_examples[inx].code) < 0)
  {
     exit(1);
  }

  flush(fd);

  while (1)
  {
     unsigned char *p = (unsigned char *)(&i);

     printf("Krabby Patty code?: ");
     scanf("%d", &i);

     
     printf("Sending 0x%02x 0x%02x 0x%02x 0x%02x\n", *p, *(p+1), *(p+2), *(p+3));
     leader_and_sync(fd);

     // Send over the two lowest order bytes in little endian order
     write_byte(fd, *p);
     write_byte(fd, *(p+1));
     write_byte(fd, *(p+2));
     write_byte(fd, *(p+3));
     flush(fd);
  }

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

void append(char *buf, unsigned char c)
{
   sprintf(buf+strlen(buf), "%02x", c);
}

/*
 * Sends over machine code.
 *
 *    pgm - program name (6 characters or less)
 *    load_address - where to store code
 *    entry_address - where to jump to
 *    code - machine code represented as 2 digit hex values in a string.
 */
int cassette_system(int fd, char *pgm, int load_address, int entry_address, char *code)
{
   unsigned char x;
   char buf[20000];
   char code_buf[20000];
   char *p, *q;

   memset(buf,'\0',sizeof(buf));
   memset(code_buf,'\0',sizeof(code_buf));

   /* Copy code to code_buf, removing spaces and newlines */
   for (p=code,q=code_buf; *p; p++)
   {
      if ( (*p != ' ') && (*p != '\n') )
      {
         *q = *p;
         q++;
      }
   }

   printf("code (%d bytes):\n%s\n\n", strlen(code_buf)/2, code_buf);


   /*
    * Filename Header
    *
    * 0x55
    * Up to 6 bytes of filename.  Padded with spaces to exactly 6 characters.
    *
    */
   APPEND(FILENAME_HEADER);
   for (p=pgm; *p && (p-pgm)<6; p++)
   {
      APPEND(*p);
   }
   while (p-pgm < 6)
   {
      APPEND(' ');
      p++;
   }


   /*
    * Data Header
    *
    * 0x3c
    * Count byte (1-256 bytes allowed).  Use 0 to for 256,
    * Load Address (LSB/MSB)
    * "count byte" many bytes
    * Checksum byte - tabulated from Load Address bytes and data bytes
    *
    */

   p = code_buf;
   while (*p)
   {
      int i, checksum;
      unsigned char x;

      APPEND(DATA_HEADER);

      i = strlen(p) / 2;
      if (i > DATA_BLOCK_MAX) {i = DATA_BLOCK_MAX;}

      APPEND(i & 0xff);
      APPEND(load_address & 0xff);
      APPEND((load_address>>8) & 0xff);

      checksum = (load_address & 0xff);
      checksum = (checksum + ((load_address>>8) & 0xff)) & 0xff;

      load_address += i;

      while (i>0)
      {
         x=(((*p&0x40)?9+(*p&0x07):(*p&0x0f))<<4)|((*(p+1)&0x40)?9+(*(p+1)&0x07):(*(p+1)&0x0f));
         APPEND(x);
         checksum = (checksum + x) & 0xff;
         i--;
         p+=2;
      }
      APPEND(checksum & 0xff);
   }

   /*
    * Entry Header
    *
    * 0x78
    * Entry Address (LSB/MSB)
    *
    */
   APPEND(ENTRY_HEADER);
   APPEND(entry_address & 0xff);
   APPEND((entry_address>>8) & 0xff);


   printf("cassette system file (%d bytes):\n%s\n\n", strlen(buf)/2, buf);

   if (leader_and_sync(fd)<0)
   {
      return(-1);
   }
   return(write_hex_string(fd, buf));
}

/*
 * Send the leader (255 0x00s) and sync byte (0xa5)
 */
int leader_and_sync(int fd)
{
   int i;

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

   return(0);
}

/*
 * Send a literal string
 *  - sends leader and sync
 *  - send the string
 *  - sends end of string byte (0x0d)
 */
int write_string(int fd, char *s)
{
   int i;
   char *p;

   if (leader_and_sync(fd)<0)
   {
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

   for (i=0; i<END_STRING_BYTE_LENGTH; i++)
   {
      if (write_byte(fd, END_STRING_BYTE)<0)
      {
         perror("Write END_STRING_BYTE failed");
         return(-1);
      }
   }
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

char* parse_machine_code(char *in)
{
   char *work = malloc(strlen(in)+1);
   char *out  = malloc(strlen(in)+1);
   char *p, *q, *r, *s, *outp;;

   strcpy(work, in);
   memset(out,'\0',strlen(in)+1);
   outp = out;

   for (p=work, q=strchr(p,'\n'); q; p=q+1, q=strchr(p,'\n'))
   {
      // Line start with a digit?
      if ( ((*p>='0')&&(*p<='9')) ||
           ((*p>='a')&&(*p<='f')) ||
           ((*p>='A')&&(*p<='F'))
         )
      {
         *q = '\0';

         // Look for first and second tab
         if ( (r=strchr(p,'\t')) && (s=strchr(r+1,'\t')) )
         {
            while (++r<s)
            {
               if (*r != ' ')
               {
                  *outp++ = *r;
               }
            }
         }
      }
   }

   free(work);

   return(out);
}
