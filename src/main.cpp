//*****Includes*********//
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include <stdlib.h>

//*******global variables********//
#define STEP 4
#define OCR_ENCODE 25

long xState = 0b00, yState = 0b00;
int xStored, yStored = 0;
int xMeasured, yMeasured = 0;
int xDiff, yDiff = 0;
bool axis = 0;

//UART SEND FUNCTION
void UART_send_int (int value)
{
	int i = 0;
	char string[4];
	char gap[8] = "----";
	itoa(value, string, 10);  //convert integer to string, radix=10 
	i = 0;
    while(string[i] != 0) /* print the String  "Hello from ATmega328p" */
    {
        while (!( UCSR0A & (1<<UDRE0))); /* Wait for empty transmit buffer*/
        UDR0 = string[i];           		 /* Put data into buffer, sends the data */
        i++;                             /* increment counter           */
		_delay_ms(10);
    }
	i = 0;
	while(gap[i] != 0) /* print the String  "Hello from ATmega328p" */
    {
        while (!( UCSR0A & (1<<UDRE0))); /* Wait for empty transmit buffer*/
        UDR0 = gap[i];           		 /* Put data into buffer, sends the data */
        i++;                             /* increment counter           */
		_delay_ms(10);
    }
}

/** ADC interrupt interprets voltages from joystick and converts them
   to number of steps the AVR needs to fake **/
ISR(ADC_vect)
{
	//CALCULATE XSTEPS BASED ON POTENTIOMETER POSITION AND PERCIEVED POSITION
	if(axis)
	{
		xMeasured = ADC;
		xDiff = xMeasured - xStored;
		axis = 0;
		ADMUX = 1;
	}
	else
	{
		yMeasured = ADC;
		yDiff = yMeasured - yStored;
		axis = 1;
		ADMUX = 0;
	}
}

/** Timer interrupt updates the output to fake rotary encoding
   and decreases the number of steps left **/
ISR(TIMER0_COMPA_vect)
{
	// Depending on whether steps is positive or negative. cycle through the states and subtract or add 1 after a cycle
	if(xDiff>STEP) 
	{
		switch(xState)
		{	
			case 0b00:
				PORTD &= ~(1<<PIN3);
				PORTD |= (1<<PIN2);
				xState = 0b01;
				break;
			case 0b01:
				PORTD |= (1<<PIN3);
				PORTD |= (1<<PIN2);
				xState = 0b11;
				break;
			case 0b11:
				PORTD |= (1<<PIN3);
				PORTD &= ~(1<<PIN2);
				xState = 0b10;
				break;
			case 0b10:
				PORTD &= ~(1<<PIN3);
				PORTD &= ~(1<<PIN2);
				xState = 0b00;
				xStored += STEP;
				break;
		}
	}
	if(xDiff<-STEP)
	{	
		switch(xState)
   		{	
			case 0b00:
				PORTD |= (1<<PIN3);
				PORTD &= ~(1<<PIN2);
				xState = 0b10;
				break;
			case 0b10:
				PORTD |= (1<<PIN3);
				PORTD |= (1<<PIN2);
				xState = 0b11;
				break;
			case 0b11:
				PORTD &= ~(1<<PIN3);
				PORTD |= (1<<PIN2);
				xState = 0b01;
				break;
			case 0b01:
				PORTD &= ~(1<<PIN3);
				PORTD &= ~(1<<PIN2);
				xState = 0b00;
				xStored -= STEP;
				break;
		}
	}
	if(yDiff>STEP)
	{
		switch(yState)
		{	
			case 0b00:
				PORTD &= ~(1<<PIN5);
				PORTD |= (1<<PIN4);
				yState = 0b01;
				break;
			case 0b01:
				PORTD |= (1<<PIN5);
				PORTD |= (1<<PIN4);
				yState = 0b11;
				break;
			case 0b11:
				PORTD |= (1<<PIN5);
				PORTD &= ~(1<<PIN4);
				yState = 0b10;
				break;
			case 0b10:
				PORTD &= ~(1<<PIN5);
				PORTD &= ~(1<<PIN4);
				yState = 0b00;
				yStored += STEP;
				break;
		}
	}
	if(yDiff<-STEP)
	{	
		switch(yState)
   		{	
			case 0b00:
				PORTD |= (1<<PIN5);
				PORTD &= ~(1<<PIN4);
				yState = 0b10;
				break;
			case 0b10:
				PORTD |= (1<<PIN5);
				PORTD |= (1<<PIN4);
				yState = 0b11;
				break;
			case 0b11:
				PORTD &= ~(1<<PIN5);
				PORTD |= (1<<PIN4);
				yState = 0b01;
				break;
			case 0b01:
				PORTD &= ~(1<<PIN5);
				PORTD &= ~(1<<PIN4);
				yState = 0b00;
				yStored -= STEP;
				break;
		}
	}
}

int main(void)
{
	//Setup outputs
	DDRD |= (1<<PIN5)|(1<<PIN4)|(1<<PIN3)|(1<<PIN2);	//Set outputs 5/4 are y, 3/2 are x
	PORTD = 0b00111100;
	DDRD |= (1<<PIN7);

	// Setup ADC
  	ADCSRA |= (1 << ADPS2) | (1 << ADPS1); // Set prescaler to 64 (Which gives us 125KHz ADC clock)
   	ADCSRA |= (1 << ADEN); // Enable ADC

	//Initialise x and y
	ADMUX = 0; // X axis
  	ADCSRA |= (1 << ADSC);
   	loop_until_bit_is_set(ADCSRA, ADIF);
   	xStored = ADC;
   	ADMUX = 1; // Y axis
   	ADCSRA |= (1 << ADSC);
   	loop_until_bit_is_set(ADCSRA, ADIF);
   	yStored = ADC;
   	ADMUX = 0;

	sei();	//Enable interrupts

	//Setup timer for encoder
	TCCR0A |= (1 << WGM01); // Enable CTC
   	TCCR0B |= (1 << CS02); // Set prescale to 100 and start timer
   	OCR0A = OCR_ENCODE; // Set compare for timer 0
   	TIMSK0 |= (1 << OCIE0A); // Enable interrupt

	//Start automatic ADC update
	ADCSRA |= (1 << ADATE); // Enable automatic trigger
   	ADCSRA |= (1 << ADIE); // Enable ADC interrupt
   	ADCSRA |= (1 << ADSC); // Start conversion

	//UART SETUP
    unsigned int ubrr = 51;
    UBRR0H = (ubrr>>8);
    UBRR0L = (ubrr);

    UCSR0C = 0x06;	// Set frame format: 8data, 1stop bit  
    UCSR0B = (1<<TXEN0);	// Enable  transmitter                 

	while(1)
	{
	}
}