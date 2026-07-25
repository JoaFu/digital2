/*
 * SLAVE.c
 *
 * Created:
 * Author: Joaquín Fuentes, Luis Arriaza
 * Description:
 * MCU2 (ESCLAVO) - Laboratorio 3: SPI Parte 1
 *
 * Lee dos señales analógicas (potenciómetros) con el ADC y las
 * transmite al maestro (MCU1) mediante SPI.
 */

/****************************************/
// Encabezado (Libraries)

#include <avr/io.h>
#include "SPI.h"
#include "adc.h"
/****************************************/
// Function prototypes

/****************************************/
// Main Function

int main(void)
{
    /* -------- Configuración SPI como ESCLAVO -------- */
    spi_config_t spiCfg = {
        .role       = SPI_ROLE_SLAVE,
        .data_order = SPI_DATA_ORDER_MSB_FIRST,
        .clock_mode = SPI_MODE0,
        .clock_div  = SPI_CLOCK_DIV16      // Se ignora en modo esclavo
    };

    SPI_Init(spiCfg);

    /* -------- Configuración ADC -------- */
    ADC_Init();

    uint8_t pot1, pot2;

    while (1)
    {
        // Lectura de ambos potenciómetros (10 bits -> 8 bits) 
        pot1 = (uint8_t)(ADC_Read(0) >> 2);
        pot2 = (uint8_t)(ADC_Read(1) >> 2);

        /* Cargar primer dato para el maestro */
        SPI_SlaveLoadData(pot1);

        /* Esperar a que el maestro lea el primer byte */
        SPI_SlaveReceive();

        /* Cargar segundo dato */
        SPI_SlaveLoadData(pot2);

        /* Esperar a que el maestro lea el segundo byte */	
        SPI_SlaveReceive();
    }

    return 0;
}

/****************************************/
// NON-Interrupt subroutines

/****************************************/
// Interrupt routines