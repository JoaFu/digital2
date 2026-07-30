/*
 * UART.c
 *
 * Created: 
 * Author: 
 * Description: Implementación UART para ATMega328P
 */

#include "UART.h"
#include <avr/io.h>
#include <avr/interrupt.h>

void initUART(void)
{
	// Configurar pines RX y TX
	DDRD &= ~(1<<DDD0);
	DDRD |=	 (1<<DDD1);
	
	UCSR0A = 0;
	// Habilitando interrupciones, habilitando RX y TX del UART0
	UCSR0B = (1<<RXCIE0) | (1<<RXEN0) | (1<<TXEN0);
	// Async, Paridad deshabilitada, 1 stop bit, 8 data bits
	UCSR0C = (1<<UCSZ01) | (1<<UCSZ00);
	// Setear UBRR0 = 103
	UBRR0 = 103;
}

void writeChar(char caracter)
{
	while (!(UCSR0A & (1<<UDRE0)));
	UDR0 = caracter;
}

void writeString(char* string)
{
	while (*string != '\0')
	{
		writeChar(*string);
		string++;
	}
}