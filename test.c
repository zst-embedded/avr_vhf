/*
    Copyright (C) 2015 <>< Charles Lohr


    Permission is hereby granted, free of charge, to any person obtaining a
	copy of this software and associated documentation files (the "Software"),
	to deal in the Software without restriction, including without limitation
	the rights to use, copy, modify, merge, publish, distribute, sublicense,
	and/or sell copies of the Software, and to permit persons to whom the
	Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included
	in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
	OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
	MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
	IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
	CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
	TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
	SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <stdio.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
#include "ntscfont.h"

void delay_ms(uint32_t time) {
  uint32_t i;
  for (i = 0; i < time; i++) {
    _delay_ms(1);
  }
}

#define NOOP asm volatile("nop" ::)

void NumToText( char * c, uint8_t a )
{
	c[0] = (a/100)+'0';
	c[1] = ((a/10)%10)+'0';
	c[2] = (a%10)+'0';
	c[3] = 0;
}
void NumToText4( char * c, uint16_t a )
{
	c[0] = (a/1000)+'0';
	c[1] = ((a/100)%10)+'0';
	c[2] = ((a/10)%10)+'0';
	c[3] = (a%10)+'0';
	c[4] = 0;
}


EMPTY_INTERRUPT(TIM0_OVF_vect );


#define NOP() do { __asm__ __volatile__ ("nop"); } while (0)

int main( )
{
	cli();

	// Setup PLL for TIM1
	CLKPR = 0x80;	/*Setup CLKPCE to be receptive*/
	CLKPR = 0x00;	/*No pll prescale*/

	PLLCSR = _BV(PLLE) | _BV( PCKE );

	// Setup PWM on TIM1
	//DDRB = _BV(1);
	DDRB = 0;

	DDRB |= _BV(3);
	DDRB |= _BV(4);
	PORTB |= _BV(3);
	PORTB |= _BV(4);

	TCCR1 = _BV(CS10);// | _BV(CTC1); //Clear on trigger.
	GTCCR |= _BV(PWM1B) |  _BV(COM1B0);// | _BV(COM1B1);
	OCR1B = 1;
	OCR1C = 3;
	DTPS1 = 0;
	DT1B = _BV(0) | _BV(4);
	DT1B = 0;

	// Setup TIM0 for line interval
	TCCR0A = 0;
	TCCR0B = _BV(CS01); // 30MHz/8 = 3.75MHz timer = 266.67ns
	TIMSK |= _BV(TOIE0);

	//CH3 on my AVR is OSCCAL=237 to 241
	OSCCAL = 251;
	#define LINETIME 15 //Linetime of 7..20 is barely valid. 
	// Linetime = H = 64usec = 240 timer ticks
	#define LINETIME_HALF  (LINETIME+120)

	#define POWERSET3

	#ifdef POWERSET1
		#define NTSC_VH  {	DDRB=0;}
		#define NTSC_HI  {	DDRB=_BV(4)|_BV(3); OCR1B = 1; TCNT1 = 0;}
		#define NTSC_LOW {	DDRB=_BV(4)|_BV(3); OCR1B = 2; TCNT1 = 0;}
	#elif defined( POWERSET2 )
		// normal;
		#define NTSC_VH  {	DDRB=0;}
		#define NTSC_HI  {	DDRB=_BV(3); }
		#define NTSC_LOW {	DDRB=_BV(4)|_BV(3); }

	#elif defined( POWERSET3 )
		// test duty cycle
		#define NTSC_VH {	DDRB=0; }
		#define NTSC_HI  {	DDRB=_BV(3); OCR1B = 1;}
		#define NTSC_LOW {	DDRB=_BV(4)|_BV(3); OCR1B = 2;}

	#elif defined( POWERSET4 )
		// swap polarity;
		#define NTSC_VH  {	DDRB=_BV(4)|_BV(3); }
		#define NTSC_HI {	DDRB=_BV(3); }
		#define NTSC_LOW  {	DDRB=0;}
	#else
		//#define NTSC_VH  {	DDRB=0; }
		//#define NTSC_HI   { DDRB=_BV(3); }
		//#define NTSC_LOW   { DDRB=_BV(4)|_BV(3); }
	#endif

	uint8_t line, i;

	#define TIMEOFFSET 0
	#define CLKOFS 0

	uint8_t frame = 0, k, ctll;
	char stdsr[8*13];
	sprintf( stdsr, "Fr: " );
	sprintf( stdsr+8, "Hello<><" );
	sprintf( stdsr+16, "World<><" );
	sprintf( stdsr+24, "        " );
	sprintf( stdsr+32, "        " );
	sprintf( stdsr+40, "        " );
	sprintf( stdsr+48, "        " );
	sprintf( stdsr+56, "        " );
	sprintf( stdsr+64, "        " );
	sprintf( stdsr+72, "        " );
	sprintf( stdsr+80, "        " );
	sprintf( stdsr+88, "        " );


	//ADMUX =/* _BV(REFS1)  |  _BV(ADLAR) | */ 1; //1 = PB2
	//ADCSRA = _BV(ADEN) | _BV(ADSC) | _BV(ADATE) | _BV(ADPS2) | _BV(ADPS1);

	#define RESETCNT {TCNT0 = LINETIME; TIFR|=_BV(TOV0); GTCCR|=PSR0;sei();}
	#define RESETCNT_HALF {TCNT0 = LINETIME_HALF; TIFR|=_BV(TOV0); GTCCR|=PSR0;sei();}
	#define WAITTCNT sleep_cpu();


	//#define LONGSYNC { NTSC_LOW; _delay_us(27.3-TIMEOFFSET); NTSC_HI; _delay_us(4.7-TIMEOFFSET-CLKOFS);}
	//#define SHORTSYNC { NTSC_LOW; _delay_us(2.35-TIMEOFFSET); NTSC_HI; _delay_us(29.65-TIMEOFFSET-CLKOFS); }
	
	#define LONGSYNC { NTSC_LOW; _delay_us(27-TIMEOFFSET); NTSC_HI; _delay_us(5-TIMEOFFSET-CLKOFS);}
	#define SHORTSYNC { NTSC_LOW; _delay_us(3-TIMEOFFSET); NTSC_HI; _delay_us(28-TIMEOFFSET-CLKOFS); }

	//#define LONGSYNC { NTSC_LOW; RESETCNT_HALF; _delay_us(27.3-TIMEOFFSET); NTSC_HI; WAITTCNT;}
	//#define SHORTSYNC { NTSC_LOW; RESETCNT_HALF; _delay_us(2.35-TIMEOFFSET); NTSC_HI; WAITTCNT; }

 	sleep_enable();
	sei();

	uint8_t hh = 0;
#define HHS 128
	uint8_t history[HHS];

	uint16_t ovax = 0; //0..1024 = 0...5v
	uint8_t  ovax8 = 0;

	
	while(1)
	{

		frame++;
		//H = 1./15625 = 64
		// H / 2 = 32

		// Long sync: H*2.5
		for( line = 0; line < 5; line++ ) { LONGSYNC; }
		
		// Short sync: H*2.5
		for( line = 0; line < 4; line++ ) { SHORTSYNC; }

		// CNT == H == 64us

		RESETCNT;
		// FIRST
	#define MAX_LINES (200)
		for( line = 0; line < MAX_LINES; line++ ) {
			_delay_us(1.65-TIMEOFFSET); // front porch
			NTSC_LOW;
			_delay_us(4.7-TIMEOFFSET); // horizontal sync
			NTSC_HI;
			_delay_us(1-TIMEOFFSET); // back porch (arbituary value)

			
			if (line < 20) {
				// Field-blanking interval of 17.5H
			} else if (line < 80) {
				if (40 < line && line < 60) {
					_delay_us(5);
					NTSC_VH;
					_delay_us(5);
					NTSC_HI;
					_delay_us(5);
					NTSC_VH;
					_delay_us(5);
					NTSC_HI;
				}

				/*NTSC_VH;
				for (uint8_t x = 0; x < line; x+=4) {
					NOP();
				}
				NTSC_HI;*/

				/*
				if (line < 22) {

				} else if (line < 30) {
					
				} else if (line < 50) {
					NTSC_VH;
					_delay_us(10-TIMEOFFSET-CLKOFS);
					NTSC_HI;
				} else if (line < 70) {
					NTSC_VH;
					_delay_us(15-TIMEOFFSET-CLKOFS);
					NTSC_HI;
				} else if (line < 100) {
					NTSC_VH;
					_delay_us(20-TIMEOFFSET-CLKOFS);
					NTSC_HI;
				} else {
					NTSC_VH;
					_delay_us(23-TIMEOFFSET-CLKOFS);
					NTSC_HI;
				}*/

			} else if (line < 125) {
				_delay_us(5);

				#define V_DELAY { NOP();NOP();NOP();NOP();NOP();NOP();NOP();NOP();NOP();NOP();NOP();NOP(); }
				#define V1 {NTSC_VH; V_DELAY; NTSC_HI; }
				#define V0 {NTSC_HI; V_DELAY; NTSC_HI; }

				// Hello World where 4x4 dots is 1 pixel
				if (line>>2 == 20) { V1;V0;V0;V1;V0;V1;V1;V1;V1;V0;V1;V0;V0;V0;V0;V1;V0;V0;V0;V0;V1;V1;V1;V1;  NTSC_HI; }
				if (line>>2 == 21) { V1;V0;V0;V1;V0;V1;V0;V0;V0;V0;V1;V0;V0;V0;V0;V1;V0;V0;V0;V0;V1;V0;V0;V1;  NTSC_HI; }
				if (line>>2 == 22) { V1;V1;V1;V1;V0;V1;V1;V1;V1;V0;V1;V0;V0;V0;V0;V1;V0;V0;V0;V0;V1;V0;V0;V1;  NTSC_HI; }
				if (line>>2 == 23) { V1;V0;V0;V1;V0;V1;V0;V0;V0;V0;V1;V0;V0;V0;V0;V1;V0;V0;V0;V0;V1;V0;V0;V1;  NTSC_HI; }
				if (line>>2 == 24) { V1;V0;V0;V1;V0;V1;V1;V1;V1;V0;V1;V1;V1;V1;V0;V1;V1;V1;V1;V0;V1;V1;V1;V1;  NTSC_HI; }

				if (line>>2 == 26) { V1;V0;V0;V0;V1;V0;V1;V1;V1;V1;V0;V1;V1;V1;V1;V0;V1;V0;V0;V0;V0;V1;V1;V1;  NTSC_HI; }
				if (line>>2 == 27) { V1;V0;V0;V0;V1;V0;V1;V0;V0;V1;V0;V1;V0;V0;V1;V0;V1;V0;V0;V0;V0;V1;V0;V0;V1;  NTSC_HI; }
				if (line>>2 == 28) { V1;V0;V1;V0;V1;V0;V1;V0;V0;V1;V0;V1;V1;V1;V1;V0;V1;V0;V0;V0;V0;V1;V0;V0;V1;  NTSC_HI; }
				if (line>>2 == 29) { V1;V0;V1;V0;V1;V0;V1;V0;V0;V1;V0;V1;V0;V1;V0;V0;V1;V0;V0;V0;V0;V1;V0;V0;V1;  NTSC_HI; }
				if (line>>2 == 30) { V0;V1;V0;V1;V0;V0;V1;V1;V1;V1;V0;V1;V0;V0;V1;V0;V1;V1;V1;V1;V0;V1;V1;V1;  NTSC_HI; }
				// up to 30*4 = 124

			} else {
			}


				
				

			
			// Use sleep interrupt to reset
			WAITTCNT;
			NTSC_HI;
			RESETCNT;
		}

		/*
		// Short sync: H*2.5
		for( line = 0; line < 5; line++ ) { SHORTSYNC; }

		// Long sync: H*2.5
		for( line = 0; line < 5; line++ ) { LONGSYNC; }
		
		// Short sync: H*2
		for( line = 0; line < 4; line++ ) { SHORTSYNC; }

		// SECOND
		for( line = 0; line < 305; line++ ) {
			_delay_us(1.65-TIMEOFFSET); // front porch
			NTSC_LOW;
			_delay_us(4.7-TIMEOFFSET); // horizontal sync/Synchronizing pulse
			NTSC_HI;
			_delay_us(5.7-TIMEOFFSET-CLKOFS); // back porch

			if (line < 20) {
				// Field-blanking interval of 17.5H
			} else if (line < 200) {
				NTSC_VH;
				_delay_us(10-TIMEOFFSET-CLKOFS);
			}

			NTSC_HI; //_delay_us(44.5);

			WAITTCNT;
			RESETCNT;
		}
		*/

		// Short sync: H*3
		for( line = 0; line < 6; line++ ) { SHORTSYNC; }

		/*
		for( line = 0; line < 39; line++ )
		{
			NTSC_LOW;
			_delay_us(4.7-TIMEOFFSET);
			NTSC_HI;

			//Do whatever you want.
			switch (line)
			{
			case 0:
				NumToText( stdsr+4, frame );
				break;
			case 1:
				ovax = ADC;
				ovax8 = ovax >> 2;
				history[hh++] = ovax8;
				hh&=sizeof(history)-1;
				ovax = ovax * 49 + (ovax>>1);
				ovax/=10;
				break;
			case 2:
				NumToText( stdsr+24, ovax/1000 );
				stdsr[27] = '.';
				break;
			case 3:
				break;
			case 5:
				NumToText4( stdsr+27, ovax );
				stdsr[27] = '.';
				break;
			}

			WAITTCNT;
			RESETCNT;
		}

		for( line = 0; line < 2; line++ )
		{
			NTSC_LOW;
			_delay_us(4.7-TIMEOFFSET);
			NTSC_HI;
			WAITTCNT;
			RESETCNT;
		}

		for( line = 0; line < 128; line++ )
		{
			NTSC_LOW; _delay_us(4.7-TIMEOFFSET); 
			NTSC_HI; _delay_us(8-TIMEOFFSET-CLKOFS);

//#define LINETEST
#ifdef LINETEST
			NTSC_VH; _delay_us(8-TIMEOFFSET-CLKOFS);
			NTSC_HI; _delay_us(44.5);
#else
			ctll = line>>2;
			for( k = 0; k < 8; k++ )
			{
			uint8_t ch = pgm_read_byte( &font_8x8_data[(stdsr[k+((ctll>>3)<<3)]<<3)] + (ctll&0x07) );
			for( i = 0; i < 8; i++ )
			{
				if( (ch&1) )
				{
					NTSC_VH;
				}
				else
				{
					NTSC_HI;
					NOOP;
				}
				ch>>=1;
				NOOP; NOOP; NOOP; NOOP;
			}
					NTSC_HI;

			}

			NTSC_HI;// _delay_us(46-TIMEOFFSET-CLKOFS);
			WAITTCNT;
			RESETCNT;
#endif
		}
		for( line = 0; line < (92+47); line++ )
		{
			NTSC_LOW;
			_delay_us(4.7-TIMEOFFSET);
			NTSC_HI;
			_delay_us(10);
			uint8_t v = history[(hh-line+HHS-1)&(HHS-1)];
			while(v--) NOOP;
			NTSC_VH;
			_delay_us(1);
			NTSC_HI;
			WAITTCNT;
			RESETCNT;
		}

		*/
	}
	
	return 0;
} 
