/*
 * Lab6_botones.c
 *
 * Created: 3/09/2026 17:47:33
 * Author : joaqu
 */ 
#define F_CPU 16000000UL

#include <avr/io.h>
#include <util/delay.h>
#include "UART.h"

#define RIGHT PC0
#define LEFT  PC1
#define DOWN  PC2
#define UP    PC3
#define A     PD6
#define B     PD2

#define NO_BUTTON 255U

static uint8_t readButton(void)
{
	if (!(PINC & (1U << RIGHT)))
	return 0U;
	else if (!(PINC & (1U << LEFT)))
	return 1U;
	else if (!(PINC & (1U << DOWN)))
	return 2U;
	else if (!(PINC & (1U << UP)))
	return 3U;
	else if (!(PIND & (1U << A)))
	return 4U;
	else if (!(PIND & (1U << B)))
	return 5U;

	return NO_BUTTON;
}

int main(void)
{
	static const char commands[6] = {
		'R', 'L', 'D', 'U', 'A', 'B'
	};

	uint8_t currentButton;
	uint8_t confirmedButton;
	uint8_t previousButton = NO_BUTTON;

	DDRC &= ~(
	(1U << RIGHT) |
	(1U << LEFT)  |
	(1U << DOWN)  |
	(1U << UP)
	);

	PORTC |=
	(1U << RIGHT) |
	(1U << LEFT)  |
	(1U << DOWN)  |
	(1U << UP);

	DDRD &= ~((1U << A) | (1U << B));
	PORTD |= (1U << A) | (1U << B);

	initUART();

	while (1)
	{
		currentButton = readButton();

		if (currentButton != previousButton)
		{
			_delay_ms(20);

			confirmedButton = readButton();

			if (confirmedButton == currentButton)
			{
				previousButton = currentButton;

				if (currentButton != NO_BUTTON)
				{
					writeChar(commands[currentButton]);
				}
			}
		}

		_delay_ms(1);
	}
}