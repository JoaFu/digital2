/*
 * adc.c
 * Author: Joaquin Fuentes ;)
 */

/****************************************/
// Encabezado (Libraries)

#include <avr/io.h>
#include "adc.h"

/****************************************/
// NON-Interrupt subroutines
void ADC_Init(void)
{
    ADMUX = (1 << REFS0); // Resultado justificado a la derecha ADLAR = 0

    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0); // Habilita el ADC y fija el prescaler en 128 (bits ADPS2:0 = 111) 
}

uint16_t ADC_Read(uint8_t channel)
{
    //Masking para el canal (solo 8)
    channel &= 0x07;
	
    ADMUX = (ADMUX & 0xF0) | channel; // Selecciona el canal
    ADCSRA |= (1 << ADSC);

    // Espera a que termine (ADSC vuelve a 0 automaticamente) 
    while (ADCSRA & (1 << ADSC));

    /* Aquí ADC ya contiene el resultado de 10 bits (ADCL primero,
     * ADCH despues, por eso se lee en este orden) */
	
    return ADC;
	
}