/*
 * lcd_hd44780_8bits.c
 *
 * Created: 16-07-2026
 * Author: Joaquín Fuentes
 * Description:
 * Librería para el controlador LCD HD44780 en modo de 8 bits.
 */

/****************************************/
// Encabezado (Libraries)

#include "lcd_hd44780_8bits.h"

/****************************************/
// Function prototypes

static void lcd_send_byte(LCD_HandleTypeDef *lcd, uint8_t rs, uint8_t value);

/****************************************/
// NON-Interrupt subroutines

/*-------------------------------------------------------
 * Envía un byte (comando o dato) al LCD
 *------------------------------------------------------*/
static void lcd_send_byte(LCD_HandleTypeDef *lcd, uint8_t rs, uint8_t value)
{
    lcd->set_rs(rs);
    lcd->write_byte(value);

    lcd->delay_us(1);

    lcd->set_en(1);
    lcd->delay_us(1);

    lcd->set_en(0);
    lcd->delay_us(1);
}

/*-------------------------------------------------------
 * Envía un comando al LCD
 *------------------------------------------------------*/
void LCD_Command(LCD_HandleTypeDef *lcd, uint8_t cmd)
{
    lcd_send_byte(lcd, 0, cmd);

    if (cmd == LCD_CMD_CLEAR_DISPLAY ||
        cmd == LCD_CMD_RETURN_HOME)
    {
        lcd->delay_ms(2);
    }
    else
    {
        lcd->delay_us(45);
    }
}

/*-------------------------------------------------------
 * Envía un dato al LCD
 *------------------------------------------------------*/
void LCD_Write(LCD_HandleTypeDef *lcd, uint8_t data)
{
    lcd_send_byte(lcd, 1, data);
    lcd->delay_us(45);
}

/*-------------------------------------------------------
 * Inicializa el LCD
 *------------------------------------------------------*/
void LCD_Init(LCD_HandleTypeDef *lcd,
              uint8_t cols,
              uint8_t rows,
              uint8_t dotsize)
{
    lcd->_cols = cols;
    lcd->_rows = rows;

    lcd->_display_function = LCD_8BIT_MODE;
    lcd->_display_function |= (rows > 1) ? LCD_2LINE : LCD_1LINE;

    if ((dotsize != 0) && (rows == 1))
    {
        lcd->_display_function |= LCD_5x10DOTS;
    }

    lcd->set_rs(0);
    lcd->set_en(0);

    /* Espera después del encendido */
    lcd->delay_ms(50);

    /* Function Set */
    LCD_Command(lcd,
                LCD_CMD_FUNCTION_SET | lcd->_display_function);
    lcd->delay_ms(5);

    LCD_Command(lcd,
                LCD_CMD_FUNCTION_SET | lcd->_display_function);
    lcd->delay_us(150);

    LCD_Command(lcd,
                LCD_CMD_FUNCTION_SET | lcd->_display_function);
    lcd->delay_us(150);

    /* Display ON/OFF */
    lcd->_display_control =
        LCD_DISPLAY_ON | LCD_CURSOR_OFF | LCD_BLINK_OFF;

    LCD_Command(lcd,
                LCD_CMD_DISPLAY_CONTROL | lcd->_display_control);

    /* Clear Display */
    LCD_Clear(lcd);

    /* Entry Mode */
    lcd->_display_mode =
        LCD_ENTRY_LEFT | LCD_ENTRY_SHIFT_OFF;

    LCD_Command(lcd,
                LCD_CMD_ENTRY_MODE_SET | lcd->_display_mode);
}

/*-------------------------------------------------------
 * Limpia el LCD
 *------------------------------------------------------*/
void LCD_Clear(LCD_HandleTypeDef *lcd)
{
    LCD_Command(lcd, LCD_CMD_CLEAR_DISPLAY);
}

/*-------------------------------------------------------
 * Lleva el cursor al Home
 *------------------------------------------------------*/
void LCD_Home(LCD_HandleTypeDef *lcd)
{
    LCD_Command(lcd, LCD_CMD_RETURN_HOME);
}

/*-------------------------------------------------------
 * Posiciona el cursor
 *------------------------------------------------------*/
void LCD_SetCursor(LCD_HandleTypeDef *lcd,
                   uint8_t col,
                   uint8_t row)
{
    static const uint8_t row_offsets[] =
    {
        0x00,
        0x40,
        0x14,
        0x54
    };

    uint8_t max_rows =
        sizeof(row_offsets) /
        sizeof(row_offsets[0]);

    if (row >= max_rows)
    {
        row = max_rows - 1;
    }

    if (row >= lcd->_rows)
    {
        row = lcd->_rows - 1;
    }

    LCD_Command(lcd,
                LCD_CMD_SET_DDRAM_ADDR |
                (col + row_offsets[row]));
}

/*-------------------------------------------------------
 * Imprime una cadena
 *------------------------------------------------------*/
void LCD_Print(LCD_HandleTypeDef *lcd,
               const char *str)
{
    while (*str)
    {
        LCD_Write(lcd, (uint8_t)*str++);
    }
}

/*-------------------------------------------------------
 * Imprime un carácter
 *------------------------------------------------------*/
void LCD_PrintChar(LCD_HandleTypeDef *lcd,
                   char c)
{
    LCD_Write(lcd, (uint8_t)c);
}

/*-------------------------------------------------------
 * Crea un carácter personalizado
 *------------------------------------------------------*/
void LCD_CreateChar(LCD_HandleTypeDef *lcd,
                    uint8_t location,
                    const uint8_t charmap[8])
{
    uint8_t i;

    /* Solo existen 8 posiciones en la CGRAM */
    location &= 0x07;

    LCD_Command(lcd,
                LCD_CMD_SET_CGRAM_ADDR |
                (location << 3));

    for (i = 0; i < 8; i++)
    {
        LCD_Write(lcd, charmap[i] & 0x1F);
    }

    /* Regresa a la DDRAM */
    LCD_SetCursor(lcd, 0, 0);
}

/*-------------------------------------------------------
 * Activa o desactiva la pantalla
 *------------------------------------------------------*/
void LCD_Display(LCD_HandleTypeDef *lcd,
                 uint8_t on)
{
    if (on)
    {
        lcd->_display_control |= LCD_DISPLAY_ON;
    }
    else
    {
        lcd->_display_control &= (uint8_t)~LCD_DISPLAY_ON;
    }

    LCD_Command(lcd,
                LCD_CMD_DISPLAY_CONTROL |
                lcd->_display_control);
}

/*-------------------------------------------------------
 * Activa o desactiva el cursor
 *------------------------------------------------------*/
void LCD_Cursor(LCD_HandleTypeDef *lcd,
                uint8_t on)
{
    if (on)
    {
        lcd->_display_control |= LCD_CURSOR_ON;
    }
    else
    {
        lcd->_display_control &= (uint8_t)~LCD_CURSOR_ON;
    }

    LCD_Command(lcd,
                LCD_CMD_DISPLAY_CONTROL |
                lcd->_display_control);
}

/*-------------------------------------------------------
 * Activa o desactiva el parpadeo del cursor
 *------------------------------------------------------*/
void LCD_Blink(LCD_HandleTypeDef *lcd,
               uint8_t on)
{
    if (on)
    {
        lcd->_display_control |= LCD_BLINK_ON;
    }
    else
    {
        lcd->_display_control &= (uint8_t)~LCD_BLINK_ON;
    }

    LCD_Command(lcd,
                LCD_CMD_DISPLAY_CONTROL |
                lcd->_display_control);
}

/*-------------------------------------------------------
 * Desplaza el contenido hacia la izquierda
 *------------------------------------------------------*/
void LCD_ScrollLeft(LCD_HandleTypeDef *lcd)
{
    LCD_Command(lcd,
                LCD_CMD_CURSOR_SHIFT |
                LCD_MOVE_DISPLAY |
                LCD_MOVE_LEFT);
}

/*-------------------------------------------------------
 * Desplaza el contenido hacia la derecha
 *------------------------------------------------------*/
void LCD_ScrollRight(LCD_HandleTypeDef *lcd)
{
    LCD_Command(lcd,
                LCD_CMD_CURSOR_SHIFT |
                LCD_MOVE_DISPLAY |
                LCD_MOVE_RIGHT);
}

/*-------------------------------------------------------
 * Configura escritura de izquierda a derecha
 *------------------------------------------------------*/
void LCD_LeftToRight(LCD_HandleTypeDef *lcd)
{
    lcd->_display_mode |= LCD_ENTRY_LEFT;

    LCD_Command(lcd,
                LCD_CMD_ENTRY_MODE_SET |
                lcd->_display_mode);
}

/*-------------------------------------------------------
 * Configura escritura de derecha a izquierda
 *------------------------------------------------------*/
void LCD_RightToLeft(LCD_HandleTypeDef *lcd)
{
    lcd->_display_mode &= (uint8_t)~LCD_ENTRY_LEFT;

    LCD_Command(lcd,
                LCD_CMD_ENTRY_MODE_SET |
                lcd->_display_mode);
}

/*-------------------------------------------------------
 * Activa o desactiva el desplazamiento automático
 *------------------------------------------------------*/
void LCD_Autoscroll(LCD_HandleTypeDef *lcd,
                    uint8_t on)
{
    if (on)
    {
        lcd->_display_mode |= LCD_ENTRY_SHIFT_ON;
    }
    else
    {
        lcd->_display_mode &= (uint8_t)~LCD_ENTRY_SHIFT_ON;
    }

    LCD_Command(lcd,
                LCD_CMD_ENTRY_MODE_SET |
                lcd->_display_mode);
}

/****************************************/
// Interrupt routines

/* No se utilizan interrupciones en esta librería */