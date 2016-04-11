/*
 * uart_out.c
 *
 *  Created on: Apr 7, 2016
 *      Author: black
 */

#include "uart_out.h"

#include <msp430.h>
#include <string.h>
#include <stdio.h>

/*
 * UART Timing Parameters
 * The UART will be driven by the SMCLK at 8Mhz
 * The desired baudrate in this example is 9600
 */
#define SMCLKRATE 8000000
#define BAUDRATE 9600
/*
 * calculate the prescalar and "modulation":
 * prescaler = CLOCK/baudrate
 * BRDIV = integer part of prescalar
 * modulation = Round (8* fractional part of prescaler)
 *
 */
#define BRDIV16 ((16*SMCLKRATE)/BAUDRATE)
#define BRDIV (BRDIV16/16)
#define BRMOD ((BRDIV16-(16*BRDIV)+1)/16)
#define BRDIVHI (BRDIV/256)
#define BRDIVLO (BRDIV-BRDIVHI*256)

//Port 1 pins used for transmit and receive are P1.2 and P1.1
// (receive is not actually used in this example)
#define TXBIT 0x04
#define RXBIT 0x02

char *uartString = NULL;

void uartInit()
{
	UCA0CTL1 = UCSWRST;   // reset and hold UART state machine
	UCA0CTL1 |= UCSSEL_2; // select SMCLK as the clock source
	UCA0BR1=BRDIVHI;      // set baud parameters, prescaler hi
	UCA0BR0=BRDIVLO;	  // prescaler lo
	UCA0MCTL=2*BRMOD;     // modulation
	// setup the TX pin (connect the P1.2 pin to the USCI)
	P1SEL |= TXBIT;
	P1SEL2|= TXBIT;
	// allow the UART state machine to operate
	UCA0CTL1 &= ~UCSWRST;
}

#pragma vector=USCIAB0TX_VECTOR
void interrupt uartTXHandler()
{
	//check if we're at the end of the string
	if (*uartString != '\0')
	{
		//if not, send it
		UCA0TXBUF = *uartString;
		//increment pointer
		uartString++;
	}
	else
	{
		//reset string pointer
		uartString = NULL;
		//finished transmitting
		IE2 &= ~UCA0TXIE;
		// wake up CPU
		__bic_SR_register_on_exit(CPUOFF);
	}
}

void uartPrint(char * aString)
{
	if (uartString == NULL)
	{
		//disable interrupts while configuring
		__bic_SR_register(GIE);
		//set string
		uartString = aString;
		//enable transmit interrupts (won't start as GIE should be off)
		IE2 |= UCA0TXIE;
  		//turn CPU off and enable interrupts
  	    __bis_SR_register(CPUOFF + GIE);
	}
}
