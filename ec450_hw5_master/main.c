// MASTER

#include <msp430.h>
#include <stdlib.h>
#include <limits.h>

#include "uart_out.h"
#include "spi.h"

#define LOW		0
#define HIGH 	1
#define BUTTON	BIT3

short lastButtonState = HIGH;

void init_button(){
	// Set button as input
	P1DIR &= ~BUTTON;
	// Set button to active high
	P1OUT |= BUTTON;
	// Activate pullup resistors on button
	P1REN |= BUTTON;
}


int genRandom() {

	TA0CTL |= TACLR;            // reset clock
	TA0CTL = TASSEL_2+ID_0+MC_1;// clock source = SMCLK
	                            // clock divider=1
	                            // UP mode
	                            // timer A interrupt off
	// compare mode, output mode 0, no interrupt enabled
	TA0CCTL0=0;
	TA0CCR0 = INT_MAX;

	volatile int i = 0;
	while (i < 1000) {
		i++;
	}

	int result = TAR;
	TA0CTL |= TACLR;

	return result;
}


interrupt void WDT_interval_handler()
{
	// read the BUTTON state
	short buttonState = (P1IN & BUTTON ? HIGH : LOW);
	//check if button when from pressed to unpressed
	//button is active high, so look for change from LOW to HIGH
	if (lastButtonState == LOW && buttonState == HIGH)
	{
		//disable watchdog
		IE1 &= ~WDTIE;
		// wake up CPU
		__bic_SR_register_on_exit(CPUOFF);
	}
	lastButtonState = buttonState;
}
ISR_VECTOR(WDT_interval_handler, ".int10")

void writeString(char *string, const char * aPrint)
{
	do
	{
		*string = *aPrint;
		string++;
		aPrint++;
	}
	while (aPrint != '\0');
}

void writeInt(char * string, int aNum)
{
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

void main(){

	WDTCTL = WDTPW + WDTHOLD;       // Stop watchdog timer
	BCSCTL1 = CALBC1_8MHZ;			// 8Mhz calibration for clock
  	DCOCTL  = CALDCO_8MHZ;

  	//seed random
  	srand(genRandom());

	// setup the watchdog timer as an interval timer
	WDTCTL = WDT_MDLY_8;

  	spiInit();
  	init_button();
  	uartInit();

  	unsigned char nextByteToSend;
  	unsigned char receivedByte;
  	unsigned char lower;
  	volatile unsigned int answer = 30;
  	volatile char state = 0;
  	short hasUARTMessage = 0;
  	char stringBuffer[20];
  	while (1)
  	{
  		if (state == 0)
  		{
  			//enable watchdog
  			IE1 |= WDTIE;
  			//wait for button
  			__bis_SR_register(LPM0_bits + GIE);
  			//button pressed, now playing game
  			answer = rand();
  			state = 1;
  		}

		switch (state) {
		case 1:		//sending R
			nextByteToSend = 'R';
			state++;
			hasUARTMessage = 1;
			writeString(&stringBuffer[0], "Starting game...\r\n");
			break;
		case 2:		//waiting for G
			nextByteToSend = '0';
			state++;
			break;
		case 3:		//sending 1
			nextByteToSend = '1';
			state++;
			break;
		case 4:		//waiting for #1 (upper byte)
			nextByteToSend = '0';
			state++;
			break;
		case 5:		//waiting for receive #1
			nextByteToSend = 'A';
			state++;
			break;
		case 6:		//sending 2
			lower = receivedByte;
			nextByteToSend = '2';
			state++;
			break;
		case 7:		//waiting for #2
			nextByteToSend = '0';
			state++;
			break;
		case 8:		//waiting for receive #2
			nextByteToSend = 'A';
			state++;
			break;
		case 9: {	//sending High(H)/Low(L)/Equal(E)
			unsigned int guess = (receivedByte << 8) + lower;
			hasUARTMessage = 1;
			if (guess > answer) {
				writeString(&stringBuffer[0], "High:");
				writeInt(&stringBuffer[5], guess);
				writeString(&stringBuffer[10], "\r\n");
				nextByteToSend = 'H';
				state = 2;
			} else if (guess < answer) {
				writeString(&stringBuffer[0], "Low:");
				writeInt(&stringBuffer[4], guess);
				writeString(&stringBuffer[9], "\r\n");
				nextByteToSend = 'L';
				state = 2;
			} else {
				writeString(&stringBuffer[0], "Equal:");
				writeInt(&stringBuffer[6],  guess);
				writeString(&stringBuffer[11], "\r\n");
				nextByteToSend = 'E';
				state = 0;
			}
			break;
		}
		case 0:		//finished, wait for button
			break;
		}

  		//receive byte
		receivedByte = spiSendByte(nextByteToSend);

  		if (hasUARTMessage)
  		{
  			//send uart info
  			uartPrint(stringBuffer);
  			hasUARTMessage = 0;
  		}
  	}
}


