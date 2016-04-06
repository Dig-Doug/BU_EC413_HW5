

#define LOW		0
#define HIGH 	1
#define BUTTON	BIT3

short lastButtonState = HIGH;

int main(void) {
	// setup the watchdog timer as an interval timer
	WDTCTL = WDT_MDLY_8;

	// enable the WDT interrupt (in the system interrupt register IE1)
	IE1 |= WDTIE;
	
	// Set button as input
	P1DIR &= ~BUTTON;
	// Set button to active high
	P1OUT |= BUTTON;
	// Activate pullup resistors on button
	P1REN |= BUTTON;
}

interrupt void WDT_interval_handler()
{
	// read the BUTTON state
	short buttonState = (P1IN & BUTTON ? HIGH : LOW);
	//check if button when from pressed to unpressed
	//button is active high, so look for change from LOW to HIGH
	if (lastButtonState == LOW && buttonState == HIGH)
	{
		//TODO
	}
	lastButtonState = b;
}