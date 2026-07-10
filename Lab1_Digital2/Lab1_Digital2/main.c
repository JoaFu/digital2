/*
 * practica1_digital2.c
 *
 * Created: 
 * Author: Joaquín Fuentes
 * Description: 
 */
/****************************************/
// Encabezado (Libraries)

#define F_CPU 16000000UL

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdbool.h>
#include "display_ordered_opcodes.h"

/****************************************/
// Function prototypes
void init_ports(void);
void init_timer0(void);
void init_pcint(void);
void leer_botones(void);
/***************************************/
// Variables
volatile uint8_t contador = 5;
volatile uint16_t ticks = 0;
volatile uint8_t mux = 0;
volatile uint8_t leds = 0x00;
volatile uint8_t flag_boton = 0;
/****************************************/
// Main Function
int main (void)
{
	init_ports();
	init_timer0();
	init_pcint();
	sei();
	
	while (1)
	{
		if (flag_boton == 1)
		{
			if (ticks >= 100)
			{
				ticks = 0;
				if (contador > 0)
				{
					contador--;
				}
				else
				{
					contador = 5;
					flag_boton = 0;
				}
		}
	};
		display_show(contador);
		
	}
	
}

/****************************************/
// NON-Interrupt subroutines
void init_ports(void){
    DDRD = 0xFF; //Display y Leds 
    PORTD = 0x00;
	
    //PB0 para display y PB1 para LEDs
	DDRB |= (1<<PB0)|(1<<PB1);
    PORTB &= ~((1<<PB0)|(1<<PB1));
	

    //Botones
    DDRC &= ~((1<<PC0)|(1<<PC1)|(1<<PC2)); //entradas 
    PORTC |= (1<<PC0)|(1<<PC1)|(1<<PC2); //pull-ups
}
void init_timer0(void){
	TCCR0A = (1<<WGM01);
	OCR0A  = 156;
	TCCR0B = (1<<CS02)|(1<<CS00);
	TIMSK0 = (1<<OCIE0A);
	
}
	
void init_pcint(void){
	PCICR |= (1 << PCIE1);
	PCMSK1 |= (1 << PCINT8) | (1 << PCINT9) | (1 << PCINT10);
	
}
/****************************************/
// Interrupt routines
ISR(TIMER0_COMPA_vect)
{	
	ticks++;
}

ISR(PCINT1_vect)
{
	if (!(PINC & (1 << PC0)))
	{
		flag_boton = 1;
	}

}