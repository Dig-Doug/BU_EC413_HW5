// MASTER

#include <msp430.h>

//Bit positions in P1 for SPI
#define SPI_CLK 0x20
#define SPI_SOMI 0x40
#define SPI_SIMO 0x80

// calculate the lo and hi bytes of the bit rate divisor
#define BIT_RATE_DIVISOR 32
#define BRLO (BIT_RATE_DIVISOR &  0xFF)
#define BRHI (BIT_RATE_DIVISOR / 0x100)

volatile unsigned int answer = 30;
volatile char state = 'G';
volatile unsigned char upper;
volatile unsigned char lower;
volatile unsigned char test[50];
volatile unsigned short i = 0;
volatile unsigned char foo;

// ===== Watchdog Timer Interrupt Handler ====
#define ACTION_INTERVAL 3
volatile unsigned int action_counter=ACTION_INTERVAL;

void sendByte(unsigned char aData)
{
	UCB0TXBUF = aData;
}

interrupt void WDT_interval_handler(){
	if (--action_counter==0){
		action_counter=ACTION_INTERVAL;

		if(i < 50){
			test[i] = foo;
			i++;
		}
		else{
			i = 0;

		}


		if(state == 'G'){
			state = '0';
			sendByte('0');
		}

		else if (state == '0'){
			state = '1';
			sendByte('1');
		}

		else if (state == '1'){
			upper = UCB0RXBUF;
			state = '2';
			sendByte('2');
		}
		else if (state == '2'){
			lower = UCB0RXBUF;
			unsigned int guess = (upper << 8) + lower;

			if(guess > answer){
				state = '1';
				sendByte('H');
			}
			else if (guess < answer){
				state = '1';
				sendByte('L');
			}
			else if (guess == answer){
				state = 'G';
				sendByte('E');
			}
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



	IFG2 &= ~UCB0RXIFG;		 // clear UCB0 RX flag
}
ISR_VECTOR(spi_rx_handler, ".int07")

void main(){

	WDTCTL = WDTPW + WDTHOLD;       // Stop watchdog timer
	BCSCTL1 = CALBC1_8MHZ;			// 8Mhz calibration for clock
  	DCOCTL  = CALDCO_8MHZ;
  	init_wdt();
  	init_spi();

  	sendByte('R');

 	_bis_SR_register(GIE+LPM0_bits);
}
