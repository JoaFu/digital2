/*
 * display_ordered_opcodes.c
 *
 * Created: 9/07/2026 17:46:18
 *  Author: joaqu
 */ 
#include "display_ordered_opcodes.h"

/* Tabla de códigos del display
   Display de cátodo común
*/

static const uint8_t displayTable[10] =
{
    0b00111111, //0
    0b00000110, //1
    0b01011011, //2
    0b01001111, //3
    0b01100110, //4
    0b01101101, //5
    0b01111101, //6
    0b00000111, //7
    0b01111111, //8
    0b01101111  //9
};

void display_show(uint8_t digit)
{
    if(digit < 10)
    {
        PORTD = displayTable[digit];
    }
}