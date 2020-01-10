#include <avr/io.h>
//#include <avr/signal.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <math.h>
#include <string.h>
#include "qCode.h"
#include <avr/wdt.h>
#define nop() \
   asm volatile ("nop")

#define	USART0_BAUD 9600
#ifndef F_CPU
   #define F_CPU 8000000UL     
#endif 

//Remember to set the cpu fuse to 8MHZ internal , because of the nature of the code ATM, the loop timigs are dictated from the Clock Frequency.

// Watchdog Function Pototype
void wdt_init(void) __attribute__((naked)) __attribute__((section(".init3")));

typedef unsigned long  u32;
typedef unsigned short u16;
typedef unsigned char  u8;

//disable this to have debug info on the serial port.
//#define RELEASE


#define LOADER_ADDR 0x40D000

#define VER2	// VER1 VER2



/* NOTE: OUT,CLK,IN must be on the same port. */
#ifdef VER1
#define X_OUT 0x10
#define X_OUT_PORT PORTB
#define X_OUT_PIN PINB
#define X_OUT_DDR DDRB
#define X_CLK 0x08
#define X_CLK_PORT PORTB
#define X_CLK_PIN PINB
#define X_CLK_DDR DDRB
#define X_IN  0x20
#define X_IN_PORT  PORTB
#define X_IN_PIN  PINB
#define X_IN_DDR  DDRB
#define X_STR 0x08
#define X_STR_PORT PORTD
#define X_STR_PIN PIND
#define X_STR_DDR DDRD

// green
#define LED1_ON   PORTB &=~0x80
#define LED1_OFF  PORTB |= 0x80
// red
#define LED2_ON   PORTC &=~0x01
#define LED2_OFF  PORTC |= 0x01
#define LED_INIT  DDRB = 0x80; DDRC = 1;


#else
//SPI_MOSI, pin 11 on the arduino
#define X_OUT 0x10
#define X_OUT_PORT PORTB
#define X_OUT_PIN PINB
#define X_OUT_DDR DDRB

//SPI_CLK, pin 13 on the arduino (with LED)
#define X_CLK 0x20
#define X_CLK_PORT PORTB
#define X_CLK_PIN PINB
#define X_CLK_DDR DDRB

//SPI_MISO, pin 12 on the arduino
#define X_IN  0x08
#define X_IN_PORT  PORTB
#define X_IN_PIN  PINB
#define X_IN_DDR  DDRB

//GPIO, pin 15 on the arduino
#define X_STR 0x02
#define X_STR_PORT PORTB
#define X_STR_PIN PINB
#define X_STR_DDR DDRB

// green PIN 2 on the arduino
#define LED1_ON   PORTD &=~0x04
#define LED1_OFF  PORTD |= 0x04
// red   Pin 3 on the arduino
#define LED2_ON   PORTD &=~0x08
#define LED2_OFF  PORTD |= 0x08
#define LED_INIT  DDRD = 0x0C;

#endif

extern const u8 qcode[];
extern const u8* qcode_end;
extern const u8 upload[];
extern const u8* upload_end;
extern const u8 credits[];
extern const u8* credits_end;

void wdt_init(void);
void reset(void);
void ldelay(volatile int i);

// Function Implementation
void wdt_init(void)
{
    MCUSR = 0;
    wdt_disable();

    return;
}


inline void delay(void)
{
  int i = 68; // 28 seems minimum, ~48 seems problem spot, 60+ for shell
               // short is good for drive patch, long good for shell loading
   while ((i--) != 0) {
     nop();
   }
}

#ifndef RELEASE

void USART_Init(void)
{
  UBRR0L = (uint8_t) (F_CPU / (16UL * USART0_BAUD)) - 1;
  UBRR0H = (uint8_t) ((F_CPU / (16UL * USART0_BAUD)) - 1) >>8;
  UCSR0B = (1<<TXEN0 | 1<<RXEN0); 	// tx/rx enable
}

void USART_Transmit( unsigned char data )
{
	if (data == '\n')
		USART_Transmit('\r');
	/* Wait for empty transmit buffer */
	while ( !( UCSR0A & (1<<UDRE0)) );
	/* Put data into buffer, sends the data */
	UDR0 = data;
}

void sputs( char* data )
{
	while (*data) USART_Transmit(*data++);
}

#else
#define sputs(x)
#endif


#ifndef RELEASE
void sputhex16(short c)
{
	char i;
	for (i=0; i<4; ++i)
		USART_Transmit("0123456789ABCDEF"[(c>>(12-(i*4)))&0xF]);
}

void sputhex8(short c)
{
	char i;
	for (i=0; i<2; ++i)
		USART_Transmit("0123456789ABCDEF"[(c>>(4-(i*4)))&0xF]);
	
	USART_Transmit(' ');
}
#else
#define sputhex16(x)		x;
#define sputhex8(x)			x;
#endif

//// xxx stuff

int ndelay = 0;


int io(char i)
{
	char res = 0;

	if (i)
		X_OUT_PORT |= X_OUT;
	else
		X_OUT_PORT &=~X_OUT;
	
   X_CLK_PORT &= ~X_CLK; 
   if (!ndelay) {
     delay();
   }
   res = X_IN_PIN & X_IN;
   X_CLK_PORT |= X_CLK;
   if (!ndelay) {
     delay();
   }

	return !!res;
}

void send8(unsigned char c)
{
  unsigned long int i = 0;

  while (X_STR_PIN & X_STR) {
    if ((i++) > 4000) {
      sputs("to in send8\n");
      reset();
    }
  }
  nop(); // glitch protection
  for (i = 0; i < 8; ++i)
  {
    if (X_STR_PIN & X_STR) {
      sputs("rst in send8 inner\n");
      reset();
    }
    io(c & (1 << i));
  }
}

unsigned char recv8(void)
{
  unsigned char x = 0;
  unsigned long int i = 0;
  int a = 0;
  
  while (!(X_STR_PIN & X_STR))
  {
    if ((i++) > 4000) {
      sputs("to in recv8\n");
      reset();
    }
  }
  nop(); // glitch protection
  for (a = 0; a < 8; ++a)
  {
    if (!(X_STR_PIN & X_STR))
    {
      sputs("rst in recv8 inner\n");
      reset();
    }
    x |= io(0) << a;
  }
  return x;
}

unsigned char io8(unsigned char c)
{
	unsigned char x = 0;
	int i;
	for (i=0; i<8; ++i)
		x |= io(c & (1<<i)) << i;
	return x;
}

/*
unsigned char recv8_nowait(void)
{
	unsigned char x = 0;
	int i;
	for (i=0; i<8; ++i)
		x |= io(0) << i;
	return x;
}
*/

int read_mem(unsigned char *dst, long addr, int len)
{
	send8(0xff);
	send8(0);
	send8(addr >> 8);
	send8(addr);
	send8(0);
	send8(addr >> 16); // high
	send8(0);
	send8(0);
	
	send8(len);
	send8(0);
	
	int err = recv8();
	err |= recv8() << 8;
	
	if (err)
	{
		sputs("error: ");
		sputhex16(err);
		sputs("\n");
		return err;
	}
	
	while (len--)
		*dst++ = recv8();

	return 0;
}

void write_word(long address, unsigned short data)
{
	send8(0xfe);
	send8(0x00);
	send8(address >> 8);
	send8(address);
	
	send8(data);
	send8(address >> 16);
	send8(data >> 8);
	send8(0);
	
	send8(2);
	send8(0);
	
	recv8(); recv8(); recv8(); recv8();
}

void write_word_norecv(long address, unsigned short data)
{
	send8(0xfe);
	send8(0x00);
	send8(address >> 8);
	send8(address);
	
	send8(data);
	send8(address >> 16);
	send8(data >> 8);
	send8(0);
	
	send8(2);
	send8(0);
}

void write_block(long address, unsigned char *source, int len)
{
	while (len >= 1) {
		write_word(address, pgm_read_byte(source) | (pgm_read_byte(source+1) << 8));
		address += 2;
		len -= 2;
		source += 2;
	}
} 

void reset()
{
  	#define soft_reset()        
   	wdt_enable(WDTO_15MS);  
	sputs("RESET!\n");
//	WDTCR = 8;
	while (1);
}

void ldelay(volatile int i)
{
	while ((i--) != 0) {
     nop();
//		if (X_STR_PIN & X_STR)
//			reset();
	}
}


/*
void sleep(int nMs)
{
	while(--nMs != 0) {
		ldelay(250);
		if (X_STR_PIN & X_STR)
			reset();
	}
}

#define BLUE	0
#define RED		1

void sleepOrg(int nMs)
{
	while(--nMs != 0) {
		ldelay(250);
	}
}

void FlashLED(u8 bLed, int nTimes)
{
	while(nTimes-->0) {
		if(bLed == 0)	LED1_ON;
		else			LED2_ON;
		sleep(10);
		if(bLed == 0)	LED1_OFF;
		else			LED2_OFF;
		sleep(10);
	}
}
*/

int main(void)
{
	const unsigned int	qcodesize	= ((((const unsigned int) &qcode_end)	- ((const  unsigned int) &qcode))	& 0xFFFE) + 2;
	const unsigned int	uploadsize	= ((((const unsigned int) &upload_end)	- ((const  unsigned int) &upload))	& 0xFFFE) + 2;
//	const unsigned int	creditssize = ((((const unsigned int) &credits_end)	- ((const  unsigned int) &credits))	& 0xFFFC) + 4;
	const unsigned int	creditssize = ((((const unsigned int) &credits_end)	- ((const  unsigned int) &credits))	& 0xFFFE) + 2;

	unsigned short last_recv=0;

	LED_INIT;
#ifndef RELEASE	
	USART_Init(); // 9600 baud
#endif
	
	int i;
	X_OUT_PORT &= ~(X_CLK|X_OUT|X_IN);
	X_STR_PORT |= X_STR;
	X_OUT_DDR = 2 | X_CLK | X_OUT | 0x80;
	X_STR_DDR &= ~X_STR;
	
	// VCC/GND not present: no leds
	// CLK missing: will be kept in first stage
	// DIN missing: will be kept in second stage
	// DOUT missing: will always reset
	// 1 0
	// 1 1
	// 0 0
	// 0 1
	LED2_ON; LED1_OFF; 	// -> red

   //ndelay starts at 0 and switches to 1 later; used in io()
	sputs("syncing..\n");
	while (1) {
		last_recv >>= 1;
		last_recv |= io(1) ? 0x8000 : 0;
		//sputhex16(last_recv); sputs("\n");
		if (last_recv == 0xeeee)
		  break;
	}
	sputs("sync ok.\n");

	// stack-friendly loading :p
	const static u8 PROGMEM pLoaderCode[] =	{	0x80, 0x00,					//  8000		MOV	$00,D0				
											0xC4, 0xDA, 0xFC,			//  C4DAFC		MOVB	D0,($FCDA)	# disable breakpoints
								
											0xF4,0x74,0x74,0x0a,0x08,	//	F47474A708  MOV	$080a74,a0		# restore original 
											0xF7,0x20,0x4C,0x80,		//	F7204C80    MOV	a0,($804c)		# inthandler
											0xF4,0x74,					//	F47400D040  MOV	QCODEIMGBASE,a0	# jump to drivecode init
											(LOADER_ADDR		& 0xFF),				
											(LOADER_ADDR >> 8	& 0xFF),
											(LOADER_ADDR >> 16	& 0xFF),		
											0xF0,0x00					//	F000        JMP	(a0)
	};

	u8* pUpload = qcode;
	u16 wUploadSize = qcodesize;

	u16 wTest = 0;
	read_mem(&wTest, 0x40D100, 2);

	if(wTest == 0x4444) {
		pUpload = credits;
		wUploadSize = creditssize;
	}

	write_block(LOADER_ADDR, upload, uploadsize);
	write_block(0x8674, pLoaderCode, sizeof(pLoaderCode));
	write_word_norecv(0x804d, 0x0086);


	char u8Ret = io8(0x00);
	ndelay = 1;
	
	ldelay(100);
	u8Ret |= io8(0x00);

	ldelay(100);
	io8((wUploadSize >> 9)&0xFF);
	ldelay(100);
	io8((wUploadSize >> 1)&0xFF);
	ldelay(100);
	io8(0);
	
	LED2_OFF;
	unsigned char r, e = (wUploadSize >> 1) & 0xFF, n, csum = 0;

	for (i=0; i < wUploadSize; ++i)	{
		ldelay(100);
		r = io8(n = pgm_read_byte(pUpload + i));
		csum += n;

		if (r != e)	{
         sputs("rst r diffs from e\n"); 
			reset();
		}

		e = n;
	}

	ldelay(100);
	pgm_read_byte(pUpload + i);

//	if(bUploadAgain) {
//		io8(0x99);
//		goto uploadAgain;
//	}
//	else 
	{
		io8(0x21);
	}

	io8(0);
	io8(0);

/*	if(pUpload == credits) {
		LED1_ON;
		LED2_ON;
		X_STR_PORT |= X_STR;
		while(1);
	}
*/
	// SUCCESS (BLUE)
	LED1_ON;
	X_STR_PORT |= X_STR;

//	if(pUpload == credits) {
//		LED1_ON;
//		LED2_ON;
//		sleepOrg(3000);
//	}

	while (!(X_STR_PIN & X_STR)) {
	}

   sputs("rst at fin (strobe)\n");
	reset();
	return 0;
}


