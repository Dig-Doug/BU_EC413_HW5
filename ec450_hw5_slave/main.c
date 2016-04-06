//SLAVE

#include <msp430.h>

#include <limits.h>

//Bit positions in P1 for SPI
#define SPI_CLK 0x20  //5
#define SPI_SOMI 0x40  //6
#define SPI_SIMO 0x80  //7

// calculate the lo and hi bytes of the bit rate divisor
#define BIT_RATE_DIVISOR 32
#define BRLO (BIT_RATE_DIVISOR &  0xFF)
#define BRHI (BIT_RATE_DIVISOR / 0x100)

#define STATE_MASTER_READY		'R'
#define STATE_READY_TO_GUESS	'G'
#define STATE_GUESS_FIRST_HALF	'1'
#define STATE_GUESS_SECOND_HALF	'2'
#define STATE_GUESS_HIGH		'H'
#define STATE_GUESS_LOW			'L'
#define STATE_GUESS_EQUAL		'E'

volatile unsigned int lastGuess = 0;
volatile unsigned int step = 0;
volatile unsigned char nextByteToSend;
volatile short needSend = 0;
volatile unsigned char data[50];
volatile unsigned char index = 0;

void sendByte(unsigned char aData)
{
	nextByteToSend = aData;
	needSend = 1;
}

void init_spi()
{
	// Reset state machine; SMCLK source;
	UCB0CTL1 = UCSWRST + UCSSEL_2;
	// Data capture on rising edge, 3-pin SPI mode, sync mode
	UCB0CTL0 = UCCKPH					// Data capture on rising edge
			   							// read data while clock high
										// lsb first, 8 bit mode,
			   +UCMODE_0				// 3-pin SPI mode
			   +UCSYNC;					// sync mode (needed for SPI or I2C)
	UCB0BR0=BRLO;						// set divisor for bit rate
	UCB0BR1=BRHI;
	UCB0CTL1 &= ~UCSWRST;				// enable UCB0 (must do this before setting
										//              interrupt enable and flags)
	// clear UCB0 RX & TX flag
	IFG2 &= ~UCB0RXIFG;
	IFG2 &= ~UCB0RXIFG;
	// enable UCB0 RX & TX interrupt
	IE2 |= UCB0RXIE;
	IE2 |= UCB0TXIE;
	// Connect I/O pins to UCB0 SPI
	P1SEL |=SPI_CLK+SPI_SOMI+SPI_SIMO;
	P1SEL2|=SPI_CLK+SPI_SOMI+SPI_SIMO;
}

#pragma vector=USCIAB0RX_VECTOR
void interrupt spi_rx_handler()
{
	volatile unsigned char dataReceived = UCB0RXBUF; // copy data to global variable

	if (dataReceived == STATE_MASTER_READY)
	{
		lastGuess = INT_MAX / 2;
		step = INT_MAX / 4;
		sendByte(STATE_READY_TO_GUESS);
	}
	else if (dataReceived == STATE_GUESS_FIRST_HALF)
	{
		unsigned char lowerHalf = lastGuess & 0xFF;
		sendByte(lowerHalf);
	}
	else if (dataReceived == STATE_GUESS_SECOND_HALF)
	{
		unsigned char upperHalf = (lastGuess >> 8);
		sendByte(upperHalf);
	}
	else if (dataReceived == STATE_GUESS_HIGH)
	{
		if (step == 0)
		{
			lastGuess--;
		}
		else
		{
			lastGuess -= step;
			step /= 2;
		}
		sendByte(STATE_READY_TO_GUESS);
	}
	else if (dataReceived == STATE_GUESS_LOW)
	{
		if (step == 0)
		{
			lastGuess++;
		}
		else
		{
			lastGuess += step;
			step /= 2;
		}
		sendByte(STATE_READY_TO_GUESS);
	}
	else if (dataReceived == STATE_GUESS_EQUAL)
	{
		//DONE!
	}
	else
	{

	}

	// clear UCB0 RX flag
	IFG2 &= ~UCB0RXIFG;
}

#pragma vector=USCIAB0TX_VECTOR
void interrupt spi_tx_handler()
{
	if (needSend)
	{
		UCB0TXBUF = 0;
		UCB0TXBUF = nextByteToSend;
		needSend = 0;
	}
	else
	{
		UCB0TXBUF = 0;
	}
}

void main(){

	 // Stop watchdog timer
	WDTCTL = WDTPW + WDTHOLD;
	// 8Mhz calibration for clock
	BCSCTL1 = CALBC1_8MHZ;
  	DCOCTL  = CALDCO_8MHZ;

  	init_spi();

  	//wait for start
 	_bis_SR_register(GIE+LPM0_bits);
}
