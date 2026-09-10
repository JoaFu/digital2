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
	DDRD |= (1U << DDD1);

	UCSR0A = 0U;

	/* Solo transmisión; no se necesita RX ni interrupción en el Nano */
	UCSR0B = (1U << TXEN0);

	/* UART asíncrono, 8 bits, sin paridad, 1 stop bit */
	UCSR0C = (1U << UCSZ01) | (1U << UCSZ00);

	UBRR0H = 0U;
	UBRR0L = 103U;
}
void writeChar(char caracter)
{
	while (!(UCSR0A & (1<<UDRE0)));
	UDR0 = caracter;
}

void writeString(const char* string)
{
	while (*string != '\0')
	{
		writeChar(*string);
		string++;
	}
}