/*
 * spi.c
 *
 *  Created on: Apr 7, 2016
 *      Author: black
 */

#include "spi.h"

#include <msp430.h>

//Bit positions in P1 for SPI
#define SPI_CLK 0x20
#define SPI_SOMI 0x40
#define SPI_SIMO 0x80

// calculate the lo and hi bytes of the bit rate divisor
#define BIT_RATE_DIVISOR 32
#define BRLO (BIT_RATE_DIVISOR &  0xFF)
#define BRHI (BIT_RATE_DIVISOR / 0x100)

unsigned char receivedData;

void spiInit(){
	UCB0CTL1 = UCSSEL_2+UCSWRST;  		// Reset state machine; SMCLK source;
	UCB0CTL0 = UCCKPH					// Data capture on rising edge
			// read data while clock high
			// lsb first, 8 bit mode,
			+UCMST					// master
			+UCMODE_0				// 3-pin SPI mode
			+UCSYNC;					// sync mode (needed for SPI or I2C)
	UCB0BR0=BRLO;						// set divisor for bit rate
	UCB0BR1=BRHI;
	UCB0CTL1 &= ~UCSWRST;				// enable UCB0 (must do this before setting
	//              interrupt enable and flags)
	IFG2 &= ~UCB0RXIFG;					// clear UCB0 RX flag
	// Connect I/O pins to UCB0 SPI
	P1SEL |=SPI_CLK+SPI_SOMI+SPI_SIMO;
	P1SEL2|=SPI_CLK+SPI_SOMI+SPI_SIMO;
}

#pragma vector=USCIAB0RX_VECTOR
void interrupt spiReceivedByte()
{
	receivedData = UCB0RXBUF;
	// disable UCB0 RX interrupt
	IE2 &= ~UCB0RXIE;
	// wake up CPU
	__bic_SR_register_on_exit(CPUOFF);
}

unsigned char spiSendByte(unsigned char aData)
{
	//disable interrupts while configuring
	__bic_SR_register(GIE);
	// enable UCB0 RX interrupt
	IE2 |= UCB0RXIE;
	//set data to send
	UCB0TXBUF = aData;
	//turn CPU off and enable interrupts
	__bis_SR_register(CPUOFF + GIE);
	return receivedData;
}

