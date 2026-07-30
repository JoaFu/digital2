/*
 * Lab3_dig2_master.c
 *
 * Created:
 * Author: Joaquín Fuentes, Luis Arriaza
 * Description:
 * MCU1 (MAESTRO) - Laboratorio 3: SPI Parte 1 + Parte 2
 *
 * Solicita continuamente las lecturas de los potenciometros al
 * esclavo mediante SPI y las envia por UART a la PC. Ademas,
 * recibe un valor de 8 bits por UART, lo muestra en sus LEDs
 * y lo retransmite al esclavo para que tambien lo muestre.
 */

/****************************************/
// Encabezado (Libraries)

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include "spi.h"
#include "UART.h"

/****************************************/
// Function prototypes

void Leds_MasterInit(void);
void Leds_MasterWrite(uint8_t value);

/****************************************/
// Variables globales

#define CMD_READ_ADC   0x01
#define CMD_SET_LED    0x02

volatile uint8_t uartValue = 0;
volatile uint8_t uartDataReady = 0;

/****************************************/
// Main Function

int main(void)
{
    Leds_MasterInit();

    spi_config_t spiCfg =
    {
        .role = SPI_ROLE_MASTER,
        .data_order = SPI_DATA_ORDER_MSB_FIRST,
        .clock_mode = SPI_MODE0,
        .clock_div = SPI_CLOCK_DIV16
    };

    SPI_Init(spiCfg);

    initUART();

    sei();

    uint8_t pot1, pot2;
    char buffer[32];

    while (1)
    {
        if (uartDataReady)
        {
            uartDataReady = 0;

            Leds_MasterWrite(uartValue);

            SPI_PORT &= ~(1 << SPI_SS);
            SPI_MasterTransceive(CMD_SET_LED);
            SPI_MasterTransceive(uartValue);
            SPI_PORT |= (1 << SPI_SS);
        }
        else
        {
            SPI_PORT &= ~(1 << SPI_SS);

            SPI_MasterTransceive(CMD_READ_ADC);
            pot1 = SPI_MasterTransceive(0xFF);
            pot2 = SPI_MasterTransceive(0xFF);

            SPI_PORT |= (1 << SPI_SS);

            sprintf(buffer, "Pot1: %3u   Pot2: %3u\r\n", pot1, pot2);
            writeString(buffer);
        }
    }
}

/****************************************/
// NON-Interrupt subroutines

void Leds_MasterInit(void)
{
    DDRD |= 0xFC;
    DDRB |= 0x03;
}

void Leds_MasterWrite(uint8_t value)
{
    PORTD = (PORTD & 0x03) | ((value & 0x3F) << 2);
    PORTB = (PORTB & 0xFC) | ((value >> 6) & 0x03);
}

/****************************************/
// Interrupt routines

ISR(USART_RX_vect)
{
    uartValue = UDR0;
    uartDataReady = 1;
}