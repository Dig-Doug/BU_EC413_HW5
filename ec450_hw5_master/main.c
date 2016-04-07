// MASTER

#include <msp430.h>
#include <stdlib.h>
#include "uart_out.h"
#include <string.h>

//Bit positions in P1 for SPI
#define SPI_CLK 0x20
#define SPI_SOMI 0x40
#define SPI_SIMO 0x80

// calculate the lo and hi bytes of the bit rate divisor
#define BIT_RATE_DIVISOR 32
#define BRLO (BIT_RATE_DIVISOR &  0xFF)
#define BRHI (BIT_RATE_DIVISOR / 0x100)

#define LOW		0
#define HIGH 	1
#define BUTTON	BIT3

// Globals for printing
unsigned int conPrint=500; // number of conversions for each print line
unsigned int conPrintCounter; // down counter for printing conversions
unsigned int conPrintSequence;// output line numbers
// a buffer to construct short text output strings
#define CBUFLEN 20
char cbuffer[CBUFLEN] = "1234567891234567891"; // buffer for output of characters


//Globals for button
short lastButtonState = HIGH;

//Globals for game
volatile unsigned int answer = 30;
volatile char state = 0;
volatile unsigned char lower;
volatile unsigned char foo;


void sendByte(unsigned char aData)
{
	UCB0TXBUF = aData;
}
void init_button(){
	// Set button as input
	P1DIR &= ~BUTTON;
	// Set button to active high
	P1OUT |= BUTTON;
	// Activate pullup resistors on button
	P1REN |= BUTTON;
}

void init_WDT(){
	// setup the watchdog timer as an interval timer
	WDTCTL = WDT_MDLY_8;

	// enable the WDT interrupt (in the system interrupt register IE1)
	IE1 |= WDTIE;
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

int genRandom() {

	TA0CTL |= TACLR;            // reset clock
	TA0CTL = TASSEL_2+ID_0+MC_1;// clock source = SMCLK
	                            // clock divider=1
	                            // UP mode
	                            // timer A interrupt off
	// compare mode, output mode 0, no interrupt enabled
	TA0CCTL0=0;
	TA0CCR0 = -1;

	volatile int i = 0;
	while (i < 1000) {
		i++;
	}

	int result = TAR;
	TA0CTL |= TACLR;

	return result;
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
	case 4:		//waiting for #1 (upper byte)
		sendByte('0');
		state++;
		break;
	case 5:		//waiting for receive #1
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
	case 8:		//waiting for receive #2
		sendByte('A');
		state++;
		break;
	case 9:{	//sending High(H)/Low(L)/Equal(E)
		unsigned int guess = (foo << 8) + lower;
		//u_print_string("Guess ");
		int digit = (guess % 10000);
		cbuffer[0] = '0' + (digit < 10 ? digit : 0);
		guess /= 10;
		digit = (guess % 1000);
		cbuffer[1] = '0' + (digit < 10 ? digit : 0);
		guess /= 10;
		digit = (guess % 100);
		cbuffer[2] = '0' + (digit < 10 ? digit : 0);
		guess /= 10;
		digit = (guess % 10);
		cbuffer[3] = '0' + (digit < 10 ? digit : 0);
		guess /= 10;
		cbuffer[4] = '0' + (digit < 10 ? digit : 0);
		guess /= 10;
		cbuffer[5] = '\0';
		tx_start_string(cbuffer);
		//u_print_string("\n");

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

interrupt void WDT_interval_handler()
{
	// read the BUTTON state
	short buttonState = (P1IN & BUTTON ? HIGH : LOW);
	//check if button when from pressed to unpressed
	//button is active high, so look for change from LOW to HIGH
	if (lastButtonState == LOW && buttonState == HIGH)
	{
		answer = rand();
		state = 1;
		sendByte('R');
	}
	lastButtonState = buttonState;
}
ISR_VECTOR(WDT_interval_handler, ".int10")

void main(){

	WDTCTL = WDTPW + WDTHOLD;       // Stop watchdog timer
	BCSCTL1 = CALBC1_8MHZ;			// 8Mhz calibration for clock
  	DCOCTL  = CALDCO_8MHZ;
  	init_spi();
  	init_button();
  	init_WDT();
  	init_USCI_UART();
  	srand(genRandom());

  	// Setup printing
  	conPrintCounter=conPrint;
  	conPrintSequence=1;
  	_bis_SR_register(GIE); //enable interrupts so we can print.

  	u_print_string("Game:");
  	u_print_string(cbuffer);

 	_bis_SR_register(LPM0_bits);
}
