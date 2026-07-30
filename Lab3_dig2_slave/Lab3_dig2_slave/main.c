/*
 * Lab3_dig2_slave.c
 *
 * Created:
 * Author: Joaquín Fuentes, Luis Arriaza
 * Description:
 * MCU2 (ESCLAVO) - Laboratorio 3: SPI Parte 1 + Parte 2
 *
 * Espera comandos enviados por el maestro mediante SPI.
 * Dependiendo del comando, envia las lecturas de dos
 * potenciometros o actualiza el valor mostrado en los LEDs.
 */

/****************************************/
// Encabezado (Libraries)

#include <avr/io.h>
#include "spi.h"
#include "adc.h"

/****************************************/
// Function prototypes

void Leds_SlaveWrite(uint8_t value);

/****************************************/
// Variables globales

#define CMD_READ_ADC   0x01
#define CMD_SET_LED    0x02

/****************************************/
// Main Function

int main(void)
{
    DDRD = 0xFF;

    spi_config_t spiCfg =
    {
        .role = SPI_ROLE_SLAVE,
        .data_order = SPI_DATA_ORDER_MSB_FIRST,
        .clock_mode = SPI_MODE0,
        .clock_div = SPI_CLOCK_DIV16
    };

    SPI_Init(spiCfg);

    ADC_Init();

    uint8_t cmd;
    uint8_t ledValue;

    /* Se mantienen actualizadas las lecturas para responder
     * inmediatamente cuando el maestro las solicite. */
    uint8_t pot1 = (uint8_t)(ADC_Read(0) >> 2);
    uint8_t pot2 = (uint8_t)(ADC_Read(1) >> 2);

    while (1)
    {
        cmd = SPI_SlaveReceive();

        if (cmd == CMD_READ_ADC)
        {
            /* Envia las lecturas disponibles */
            SPI_SlaveLoadData(pot1);
            SPI_SlaveReceive();

            SPI_SlaveLoadData(pot2);
            SPI_SlaveReceive();

            /* Actualiza las lecturas para la siguiente solicitud */
            pot1 = (uint8_t)(ADC_Read(0) >> 2);
            pot2 = (uint8_t)(ADC_Read(1) >> 2);
        }
        else if (cmd == CMD_SET_LED)
        {
            ledValue = SPI_SlaveReceive();
            Leds_SlaveWrite(ledValue);
        }
    }
}

/****************************************/
// NON-Interrupt subroutines

void Leds_SlaveWrite(uint8_t value)
{
    PORTD = value;
}

/****************************************/
// Interrupt routines