/*
 * adc.h
 * Author: Joaquin Fuentes ;)
 *
 * Librería para el módulo ADC del ATmega328P.
 */

#ifndef ADC_H
#define ADC_H

#include <stdint.h>

/*-------------------------------------------------------
 * API pública
 *------------------------------------------------------*/

/*
 * Configura el ADC:
 * - Referencia: AVCC
 * - Resultado justificado a la derecha
 * - Prescaler: 128
 */
void ADC_Init(void);

/*
 * Lee el canal ADC indicado.
 *
 * Retorna:
 *   Valor ya mapeado de 10 bits (0 - 1023).
 */
uint16_t ADC_Read(uint8_t channel);

#endif /* ADC_H */