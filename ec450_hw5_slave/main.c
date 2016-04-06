//SLAVE

#include <msp430.h>

#include <limits.h>

//Bit positions in P1 for SPI
#define SPI_CLK 0x20
#define SPI_SOMI 0x40
#define SPI_SIMO 0x80

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
volatile unsigned char data[50];
volatile unsigned short dataIndex = 3;

void sendByte(unsigned char aData)
{
	UCB0TXBUF = aData;
}

void init_spi(){
	UCB0CTL1 = UCSSEL_2+UCSWRST;  		// Reset state machine; SMCLK source;
	UCB0CTL0 = UCCKPH					// Data capture on rising edge
			   							// read data while clock high
										// lsb first, 8 bit mode,
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

interrupt void WDT_interval_handler(){
	if (--dataIndex==0)
	{
		dataIndex = 10;

		unsigned char dataReceived = data[0];

		if (dataReceived == STATE_MASTER_READY)
		{
			lastGuess = INT_MAX / 2;
			step = INT_MAX / 4;
			sendByte(STATE_READY_TO_GUESS);
		}
		else if (dataReceived == STATE_GUESS_FIRST_HALF)
		{
			unsigned char upperHalf = (lastGuess >> 8);
			sendByte(upperHalf);
		}
		else if (dataReceived == STATE_GUESS_SECOND_HALF)
		{
			unsigned char lowerHalf = lastGuess & 0xFF;
			sendByte(lowerHalf);
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
	}
}
ISR_VECTOR(WDT_interval_handler, ".int10")

void init_wdt(){
	// setup the watchdog timer as an interval timer
	// INTERRUPT NOT YET ENABLED!
  	WDTCTL =(WDTPW +		// (bits 15-8) password
     	                   	// bit 7=0 => watchdog timer on
       	                 	// bit 6=0 => NMI on rising edge (not used here)
                        	// bit 5=0 => RST/NMI pin does a reset (not used here)
           	 WDTTMSEL +     // (bit 4) select interval timer mode
  		     WDTCNTCL  		// (bit 3) clear watchdog timer counter
  		                	// bit 2=0 => SMCLK is the source
  		                	// bits 1-0 = 10=> source/512
 			 );
  	IE1 |= WDTIE; // enable WDT interrupt
 }


void interrupt spi_rx_handler()
{
	unsigned char dataReceived = UCB0RXBUF; // copy data to global variable
	data[0] = dataReceived;
	// clear UCB0 RX flag
	IFG2 &= ~UCB0RXIFG;
}
ISR_VECTOR(spi_rx_handler, ".int07")

void main(){

	 // Stop watchdog timer
	WDTCTL = WDTPW + WDTHOLD;
	// 8Mhz calibration for clock
	BCSCTL1 = CALBC1_8MHZ;
  	DCOCTL  = CALDCO_8MHZ;

  	init_spi();
  	init_wdt();

  	//wait for start
 	_bis_SR_register(GIE+LPM0_bits);
}
