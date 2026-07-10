/*
 * display_ordered_opcodes.h
 *
 * Created: 9/07/2026 17:29:54
 *  Author: joaqu
 */ 
#ifndef DISPLAY_H_
#define DISPLAY_H_

#include <avr/io.h>
#include <stdint.h>

/* Control del transistor */

#define DISPLAY_ON()    (PORTB |=  (1<<PB0))
#define DISPLAY_OFF()   (PORTB &= ~(1<<PB0))

/* Funciones */

void display_show(uint8_t digit);

#endif