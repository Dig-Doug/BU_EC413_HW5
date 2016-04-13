// MASTER

#include <msp430.h>
#include <stdlib.h>
#include "uart_out.h"
#include <string.h>
#include <time.h>

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

#define RED_LED BIT0

volatile short hasUARTMessage = 0;
char stringBuffer[20];

//Globals for button
short lastButtonState = HIGH;

//Globals for game
volatile unsigned int answer = 30;
volatile char state = 0;
volatile unsigned char lower;
volatile unsigned char receivedByte;
volatile unsigned char nextByteToSend;
volatile char playing = 0;

void writeString(char *string, const char * aPrint, short length)
{
	//copied the string into the string buffer
	do
	{
		*string = *aPrint;
		string++;
		aPrint++;
		length--;
	}while (length > 0);
}

void writeInt(char * string, int aNum)
{
	//reads the number and writes each digit to the string buffer
	//terminates with a null character
	string[5] = '\0';
	int digit = aNum % 10;
	string[4] = '0' + digit;
	aNum /= 10;
	digit = aNum % 10;
	string[3] = '0' + digit;
	aNum /= 10;
	digit = aNum % 10;
	string[2] = '0' + digit;
	aNum /= 10;
	digit = aNum % 10;
	string[1] = '0' + digit;
	aNum /= 10;
	digit = aNum % 10;
	string[0] = '0' + digit;
	aNum /= 10;
}

void sendByte(unsigned char aData)
{
	//queues the byte to send
	nextByteToSend = aData;
	//UCB0TXBUF = aData;
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

void interrupt spi_rx_handler(){
	receivedByte = UCB0RXBUF; // copy data to global variable

	switch(state){
	case 1:		//sending R
		sendByte('R');
		hasUARTMessage = 1;
		writeString(&stringBuffer[0], "Game Start \r\n", 13);
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
		lower = receivedByte;
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
		unsigned int guess = (receivedByte << 8) + lower;
		hasUARTMessage = 1;
		if(guess > answer){
			writeString(&stringBuffer[0], "High:", 5);
			writeInt(&stringBuffer[5], guess);
			writeString(&stringBuffer[10], "\r\n", 3);
			sendByte('H');
			state = 2;
		}
		else if (guess < answer){
			writeString(&stringBuffer[0], "Low:", 4);
			writeInt(&stringBuffer[4], guess);
			writeString(&stringBuffer[9], "\r\n", 3);
			sendByte('L');
			state = 2;
		}
		else{
			writeString(&stringBuffer[0], "Equal:", 6);
			writeInt(&stringBuffer[6],  guess);
			writeString(&stringBuffer[11], "\r\n", 3);
			sendByte('E');
			playing = 0;
			state = 0;
			//turn led off
			P1OUT &= ~RED_LED;
			//turn on watchdog for button
			IE1 |= WDTIE;
		}
		break;
	}
	case 0:		//finished, wait for button
		break;
	}

	IFG2 &= ~UCB0RXIFG;		 // clear UCB0 RX flag
	IE2 &= ~UCB0RXIE;
	UCB0TXBUF = nextByteToSend;

	// wake up CPU
	__bic_SR_register_on_exit(CPUOFF);
}
ISR_VECTOR(spi_rx_handler, ".int07")

interrupt void WDT_interval_handler()
{
	// read the BUTTON state
	short buttonState = (P1IN & BUTTON ? HIGH : LOW);
	//check if button when from pressed to unpressed
	//button is active high, so look for change from LOW to HIGH
	if (lastButtonState == LOW && buttonState == HIGH && playing == 0)
	{
		answer = rand();
		state = 1;
		playing = 1;
		UCB0TXBUF = 'R';
		//turn led on
		P1OUT |= RED_LED;
		//turn off watchdog
		IE1 &= WDTIE;
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
	uartInit();

	//output to led
	P1DIR |= RED_LED;

	//turn led off
	P1OUT &= ~RED_LED;

	srand(time(NULL));

	while(1)
	{
		_bis_SR_register(LPM0_bits + GIE);
		if (hasUARTMessage)
		{
			//send uart info
			uartPrint(stringBuffer);
			hasUARTMessage = 0;
		}

		__bic_SR_register(GIE);
		IE2 |= UCB0RXIE;

	}

}
