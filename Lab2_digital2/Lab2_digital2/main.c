/*
 * Lab2_LCD.c
 *
 * Created: 18-07-2026
 * Author: Joaquín Fuentes
 * Description: Implementación del display LCD, ADC y UART
 */

/****************************************/
// Encabezado (Libraries)

#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdint.h>
#include <stdio.h>
#include "UART.h"
#include "adc.h"
#include "lcd_hd44780_8bits.h"

/****************************************/
// Function prototypes

static void mcu_set_rs(uint8_t state);
static void mcu_set_en(uint8_t state);
static void mcu_write_byte(uint8_t value);
static void mcu_delay_us(uint16_t us);
static void mcu_delay_ms(uint16_t ms);
void timer1_init(void);

volatile uint8_t tick_flag = 0;
volatile uint8_t uart_rx_flag = 0;
volatile char uart_rx_char = 0;
int16_t counter = 0;

/****************************************/
// Main Function

int main(void)
{
    DDRD |= (1 << PD2) | (1 << PD3) | (1 << PD4) | (1 << PD5) | (1 << PD6) | (1 << PD7);
    DDRB |= (1 << PB0) | (1 << PB1) | (1 << PB2) | (1 << PB3);

    LCD_HandleTypeDef lcd;
    lcd.set_rs     = mcu_set_rs;
    lcd.set_en     = mcu_set_en;
    lcd.write_byte = mcu_write_byte;
    lcd.delay_us   = mcu_delay_us;
    lcd.delay_ms   = mcu_delay_ms;

    ADC_Init();
    initUART();
    LCD_Init(&lcd, 16, 2, 0);
    timer1_init();
    sei();

    char buffer_top[17];
    char buffer_bottom[17];
    char uart_buffer[32];

    sprintf(buffer_bottom, "CNT:%d", counter);
    LCD_SetCursor(&lcd, 0, 1);
    LCD_Print(&lcd, buffer_bottom);

    while (1)
    {
        if (tick_flag)
        {
            tick_flag = 0;

            uint16_t raw1 = ADC_Read(0);
            uint16_t raw2 = ADC_Read(1);

            uint32_t mv = ((uint32_t)raw1 * 5000) / 1023;
            uint16_t intpart = mv / 1000;
            uint16_t decpart = (mv % 1000) / 10;

            sprintf(buffer_top, "V1:%u.%02u D2:%4u", intpart, decpart, raw2);
            LCD_SetCursor(&lcd, 0, 0);
            LCD_Print(&lcd, buffer_top);

            sprintf(uart_buffer, "V1:%u.%02u,D2:%u\r\n", intpart, decpart, raw2);
            writeString(uart_buffer);
        }

        if (uart_rx_flag)
        {
            uart_rx_flag = 0;
            char c = uart_rx_char;

            if (c == '+')
            {
                counter++;
            }
            else if (c == '-')
            {
                counter--;
            }

            sprintf(buffer_bottom, "CNT:%d   ", counter);
            LCD_SetCursor(&lcd, 0, 1);
            LCD_Print(&lcd, buffer_bottom);
        }
    }

    return 0;
}

/****************************************/
// NON-Interrupt subroutines

static void mcu_set_rs(uint8_t state)
{
    if (state) PORTD |= (1 << PD2);
    else       PORTD &= ~(1 << PD2);
}

static void mcu_set_en(uint8_t state)
{
    if (state) PORTD |= (1 << PD3);
    else       PORTD &= ~(1 << PD3);
}

static void mcu_write_byte(uint8_t value)
{
    PORTD = (PORTD & 0x0F) | ((value & 0x0F) << 4);
    PORTB = (PORTB & 0xF0) | ((value >> 4) & 0x0F);
}

static void mcu_delay_us(uint16_t us)
{
    while (1)
    {
        uint16_t temp = us;
        us--;
        if (temp == 0)
            break;
        _delay_us(1);
    }
}

static void mcu_delay_ms(uint16_t ms)
{
    while (1)
    {
        uint16_t temp = ms;
        ms--;
        if (temp == 0)
            break;
        _delay_ms(1);
    }
}

void timer1_init(void)
{
    TCCR1A = 0;
    TCCR1B = (1 << WGM12) | (1 << CS12) | (1 << CS10);
    OCR1A = 3124;
    TIMSK1 |= (1 << OCIE1A);
}

/****************************************/
// Interrupt routines

ISR(TIMER1_COMPA_vect)
{
    tick_flag = 1;
}

ISR(USART_RX_vect)
{
    uart_rx_char = UDR0;
    uart_rx_flag = 1;
}