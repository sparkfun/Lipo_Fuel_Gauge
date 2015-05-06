/*
    6-22-11
	Aaron Weiss
	aaron at sparkfun dot com
	OSHW 1.0 License, http://freedomdefined.org/OSHW
	
	LiPo Fuel Gauge Example Code
	
	Uses the MAX17043
	ATmega328@3.3V w/ external 8MHz resonator
	HF: 0xDA, LF: 0xFF, EF: 0xF8
	
	PIN CONNECTIONS:
	VCC - No connect or connect to system power
	GND - GND
	SDA - PC4
	SCL - PC5
	ALT - PD2
	QST - GND or a hardware reset pin
	
	Default Baud Rate: 9600bps 8N1
	
	Firmware: v10
	Hardware: v10
	
	Usage: Displays the raw A/D value in mV and percentage of 
		   charge every second
	
*/

#include <stdlib.h>
#include <stdio.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include "types.h"
#include "defs.h"
#include "i2c.h"

#define FOSC 8000000
#define BAUD 9600

#define sbi(var, mask)   ((var) |= (uint8_t)(1 << mask))
#define cbi(var, mask)   ((var) &= (uint8_t)~(1 << mask))

#define STATUS_LED 5

///===========Main Prototypes=============================///////////////////////
void config(void);
void power_on_reset(void);
long read_ad(void);
long read_config(void);
void read_percent(void);
void quick_start_reset(void);

/////=====================================================///////////////////////

///============Initialize Prototypes=====================///////////////////////
void ioinit(void);      // initializes IO
void UART_Init(unsigned int ubrr);
static int uart_putchar(char c, FILE *stream);
uint8_t uart_getchar(void);
static FILE mystdout = FDEV_SETUP_STREAM(uart_putchar, NULL, _FDEV_SETUP_WRITE);
void delay(uint16_t x); // general purpose delay
/////===================================================////////////////////////

///============I2C Prototypes=============//////////////////
void i2cSendStart(void);
void i2cSendStop(void);
void i2cWaitForComplete(void);
void i2cSendByte(unsigned char data);
void i2cInit(void);
void i2cHz(long uP_F, long scl_F);

///===========Global Vars=============================//////////////////////////
volatile long i=0; //counts per second
volatile double percent_decimal;
/////===================================================////////////////////////

//=========MAIN================/////////////////////////////////////////////////
ISR (INT0_vect) 
{	
	//when charge drops below 32% turn on status LED
	sbi(PORTB, STATUS_LED); 
}

ISR(TIMER1_OVF_vect)
{
	//one second timer, prints values every second
	TCNT1 = 34286;
	//printf("config=%5ld, ", read_config());
	printf("a/d=%5ld, ", read_ad());
	read_percent();
	
}

int main(void)
{	
	ioinit(); //Setup IO pins and defaults
	power_on_reset();    // reset MAX module upon MCU reset
	config();
	
	while(1)
	{	
		//put your additional code here
	}
	return (0);
}

void config(void)
{
	/////////write config register///////////////
	
	i2cSendStart();
	i2cWaitForComplete();
	i2cSendByte(0x6C);    //write MAX
	i2cWaitForComplete();
	i2cSendByte(0x0C);    //mode register
	i2cWaitForComplete();
	i2cSendByte(0x97); 
	i2cWaitForComplete();
	i2cSendByte(0x00);    //set alert to 32%
	i2cWaitForComplete();
	i2cSendStop();
	i2cSendStart();	
}

void power_on_reset(void)
{
	/////////write command register to reset///////////////
	
	i2cSendStart();
	i2cWaitForComplete();
	i2cSendByte(0x6C);    //write MAX
	i2cWaitForComplete();
	i2cSendByte(0xFE);    //mode register
	i2cWaitForComplete();
	i2cSendByte(0x54); 
	i2cWaitForComplete();
	i2cSendByte(0x00);    
	i2cWaitForComplete();
	i2cSendStop();
	i2cSendStart();	
}

long read_ad(void)
{		
	//0x6C write, 0x6D read
	
	uint16_t xm, xl;
	long temp, xo;
	
	i2cSendStart();
	i2cWaitForComplete();
	i2cSendByte(0x6C);    //write MAX
	i2cWaitForComplete();
	i2cSendByte(0x02);    //mode register
	i2cWaitForComplete();
	i2cSendStop();
	
	i2cSendStart();
	i2cWaitForComplete();
	i2cSendByte(0x6D);    //read from MAX
	i2cWaitForComplete();
	i2cReceiveByte(TRUE);
	i2cWaitForComplete();
	xm = i2cGetReceivedByte();	//high byte
	i2cWaitForComplete();
	i2cReceiveByte(TRUE);
	i2cWaitForComplete();
	xl = i2cGetReceivedByte();	//high byte
	i2cWaitForComplete();
	i2cReceiveByte(TRUE);	
	i2cSendStop();
	i2cSendStart();
	
	temp = ((xl|(xm << 8)) >> 4);
	xo = 1.25* temp;
	
	//returns A/D value in mV
	return xo;
}

long read_config(void)
{		
	///should read 38656 or 0x9700 before alert
	///should read 38752 or 0x9760 after alert

	//0x6C write, 0x6D read
	
	uint16_t xm, xl;
	long xo;
	
	//////////read command register/////////////
	
	i2cSendStart();
	i2cWaitForComplete();
	i2cSendByte(0x6C);    //write MAX
	i2cWaitForComplete();
	i2cSendByte(0x0C);    //mode register
	i2cWaitForComplete();
	i2cSendStop();
	
	i2cSendStart();
	i2cWaitForComplete();
	i2cSendByte(0x6D);    //read from MAX
	i2cWaitForComplete();
	i2cReceiveByte(TRUE);
	i2cWaitForComplete();
	xm = i2cGetReceivedByte();	//high byte
	i2cWaitForComplete();
	i2cReceiveByte(TRUE);
	i2cWaitForComplete();
	xl = i2cGetReceivedByte();	//high byte
	i2cWaitForComplete();
	i2cReceiveByte(TRUE);	
	i2cSendStop();
	i2cSendStart();	
	
	xo = xl|(xm << 8);
	
	return xo;
}

void read_percent(void)
{		
	//0x6C write, 0x6D read
	
	uint8_t xm, xl;
	//uint8_t xmh, xml, xlh, xll;
	//long xo;
	
	i2cSendStart();
	i2cWaitForComplete();
	i2cSendByte(0x6C);    //write MAX
	i2cWaitForComplete();
	i2cSendByte(0x04);    //SOC register
	i2cWaitForComplete();
	i2cSendStop();
	
	i2cSendStart();
	i2cWaitForComplete();
	i2cSendByte(0x6D);    //read from MAX
	i2cWaitForComplete();
	i2cReceiveByte(TRUE);
	i2cWaitForComplete();
	xm = i2cGetReceivedByte();	//high byte
	i2cWaitForComplete();
	i2cReceiveByte(TRUE);
	i2cWaitForComplete();
	xl = i2cGetReceivedByte();	//high byte
	i2cWaitForComplete();
	i2cReceiveByte(TRUE);	
	i2cSendStop();
	i2cSendStart();
	
	//percent_decimal = (0.003906)*xl + xm;
	//xo = xl|(xm << 8);
	printf("percent=%d\n\r", xm);
	//printf("percent=%4.2f\n\r", percent_decimal);
}

void quick_start_reset(void)
{
	i2cSendStart();
	i2cWaitForComplete();
	i2cSendByte(0x6C);    //write MAX
	i2cWaitForComplete();
	i2cSendByte(0x06);    //mode register
	i2cWaitForComplete();
	i2cSendByte(0x40); 
	i2cWaitForComplete();
	i2cSendByte(0x00);    
	i2cWaitForComplete();
	i2cSendStop();
	i2cSendStart();	
}

////////////////////////////////////////////////////////////////////////////////
///==============Initializations=======================================/////////
////////////////////////////////////////////////////////////////////////////////
void ioinit (void)
{
	//1 = output, 0 = input
	DDRB = 0b11101111; //PB4 = MISO 
	DDRC = 0b11111111; //Output on PORTC0, PORTC4 (SDA), PORTC5 (SCL), all others are inputs
	DDRD = 0b11110010; //PORTD (RX on PD0), input on PD2 ,out on PD5 for ATtiny motor enable

	PORTD |= (1<<PORTD2); //pullup on PD2 
	PORTC = (1<<PORTC4)|(1<<PORTC5); //i2c pullups
	
	cbi(PORTB, STATUS_LED);

	UART_Init((unsigned int)((1000000)/(BAUD*2)-1));
	
	i2cInit();

	//pin change interrupt on INT0
	EICRA = (1<<ISC01);//falling edge generates interrupt
	EIMSK = (1<<INT0);
	
	// Setting Timer 1:
	// normal mode
	TCCR1A = 0x00;
	// Set to clk/256 
	TCCR1B |= (1<<CS12);
	//enable overflow interrupt
	TIMSK1 |= (1<<TOIE1);
	//load timer with a value to optimize for 1 second, 
	//(256/8MHz)*(65536bits-34286) = 1s
	TCNT1 = 34286;
	
	sei(); //turn on global interrupts
}

void UART_Init(unsigned int ubrr)
{
	// Set baud rate 
	UBRR0H = ubrr>>8;
	UBRR0L = ubrr;
	
	// Enable receiver and transmitter 
	UCSR0A = (0<<U2X0);
	UCSR0B = (1<<RXEN0)|(1<<TXEN0);
	
	// Set frame format: 8 bit, no parity, 1 stop bit,   
	UCSR0C = (1<<UCSZ00)|(1<<UCSZ01);
	
	stdout = &mystdout; //Required for printf init
	
	delay(2000);
}

static int uart_putchar(char c, FILE *stream)
{
    if (c == '\n') uart_putchar('\r', stream);
  
    loop_until_bit_is_set(UCSR0A, UDRE0);
    UDR0 = c;
    
    return 0;
}

uint8_t uart_getchar(void)
{
    while( !(UCSR0A & (1<<RXC0)) );
    return(UDR0);
}

//General short delays
void delay(uint16_t x)
{
  uint8_t y;
  for ( ; x > 50 ; x--){
    for ( y = 0 ; y < 200 ; y++){ 
    asm volatile ("nop");
    }
  }
}