/*
 * main.c - MCU1 (MAESTRO)
 * Laboratorio 3: SPI - Parte 1
 *
 */

#include <avr/io.h>
#include <stdio.h>
#include "SPI.h"
#include "adc.h"
#include "UART.h"

// Functions prototypes


int main(void)
{
    /* -------- Configuracion SPI como MAESTRO -------- */
    spi_config_t spiCfg = {
        .role       = SPI_ROLE_MASTER,
        .data_order = SPI_DATA_ORDER_MSB_FIRST,
        .clock_mode = SPI_MODE0,
        .clock_div  = SPI_CLOCK_DIV16   /* 16MHz/16 = 1MHz de reloj SPI */
    };
    SPI_Init(spiCfg);

    /* -------- Configuracion UART hacia la PC -------- */
    initUART();

    uint8_t pot1, pot2;
    char buffer[32];

    while (1)
    {
        /* --- Seleccionamos al esclavo (SS en bajo) --- */
        SPI_PORT &= ~(1 << SPI_SS);

        /* 1ra transaccion: el esclavo responde con la lectura de Pot1.
         * El byte que enviamos (0xFF) es "dummy", solo sirve para
         * generar los pulsos de reloj que el esclavo necesita. */
        pot1 = SPI_MasterTransceive(0xFF);

        /* 2da transaccion: el esclavo responde con la lectura de Pot2 */
        pot2 = SPI_MasterTransceive(0xFF);

        /* --- Liberamos al esclavo (SS en alto) --- */
        SPI_PORT |= (1 << SPI_SS);

        /* --- Enviamos los valores recibidos a la terminal serial --- */
        sprintf(buffer, "Pot1: %3u   Pot2: %3u\r\n", pot1, pot2);
        writeString(buffer);
    }
}
