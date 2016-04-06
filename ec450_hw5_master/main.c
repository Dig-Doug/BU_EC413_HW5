// MASTER

#include <msp430.h>
#include <time.h>
#include <stdlib.h>

//Bit positions in P1 for SPI
#define SPI_CLK 0x20
#define SPI_SOMI 0x40
#define SPI_SIMO 0x80

// calculate the lo and hi bytes of the bit rate divisor
#define BIT_RATE_DIVISOR 32
#define BRLO (BIT_RATE_DIVISOR &  0xFF)
#define BRHI (BIT_RATE_DIVISOR / 0x100)

volatile unsigned int answer = 30;
volatile char state = 0;
volatile unsigned char lower;
volatile unsigned char foo;


void sendByte(unsigned char aData)
{
	UCB0TXBUF = aData;
}

void init_spi(){
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
	IE2 |= UCB0RXIE;					// enable UCB0 RX interrupt
	// Connect I/O pins to UCB0 SPI
	P1SEL |=SPI_CLK+SPI_SOMI+SPI_SIMO;
	P1SEL2|=SPI_CLK+SPI_SOMI+SPI_SIMO;
}

void interrupt spi_rx_handler(){
	foo = UCB0RXBUF; // copy data to global variable

	switch(state){
	case 1:		//sending R
		sendByte('R');
		state++;
		break;
	case 2:		//waiting for G
		sendByte('0');
		state++;
		break;
	case 3:		//sending 1
		sendByte('1');
		state++;
		break;
	case 4:		//waiting for #1
		sendByte('0');
		state++;
		break;
	case 5:		//receive #1
		sendByte('A');
		state++;
		break;
	case 6:		//sending 2
		lower = foo;
		sendByte('2');
		state++;
		break;
	case 7:		//waiting for #2
		sendByte('0');
		state++;
		break;
	case 8:		//reive #2
		sendByte('A');
		state++;
		break;
	case 9:{	//sending H/L/E
		unsigned int guess = (foo << 8) + lower;
		if(guess > answer){
			sendByte('H');
			state = 2;
		}
		else if (guess < answer){
			sendByte('L');
			state = 2;
		}
		else{
			sendByte('E');
			state = 0;
		}
		break;
	}
	case 0:		//finished, wait for button
		break;
	}

	IFG2 &= ~UCB0RXIFG;		 // clear UCB0 RX flag
}
ISR_VECTOR(spi_rx_handler, ".int07")

void main(){

	WDTCTL = WDTPW + WDTHOLD;       // Stop watchdog timer
	BCSCTL1 = CALBC1_8MHZ;			// 8Mhz calibration for clock
  	DCOCTL  = CALDCO_8MHZ;
  	init_spi();

  	//generate random number
  	srand(time(NULL));
  	answer = rand();

  	sendByte('R');
  	state = 1;

 	_bis_SR_register(GIE+LPM0_bits);
}
