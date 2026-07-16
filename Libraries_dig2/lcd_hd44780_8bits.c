/* =====================================================================
 * lcd_hd44780_8bits.c
 * ---------------------------------------------------------------------
 * Implementacion de la libreria LCD HD44780 en modo de 8 bits.
 *
 * A diferencia del modo de 4 bits, aqui no existe ambiguedad de
 * nibbles: cada byte (comando o dato) se coloca completo en D0-D7 y
 * se genera un solo pulso de Enable para que el LCD lo capture. Por
 * eso el algoritmo de inicializacion es mas corto que el de 4 bits.
 * ===================================================================== */

#include "lcd_hd44780_8bits.h"

/* -----------------------------------------------------------------
 * Funcion privada (uso interno de la libreria)
 * ----------------------------------------------------------------- */

/* Envia un byte completo (comando o dato) por D0-D7 y genera el pulso
 * de Enable necesario para que el LCD lo capture. 'rs' indica si es
 * comando (0) o dato (1) */
static void lcd_send_byte(LCD_HandleTypeDef *lcd, uint8_t rs, uint8_t value)
{
    lcd->set_rs(rs);
    lcd->write_byte(value);
    lcd->delay_us(1);        /* Tiempo de setup de datos antes de E */
    lcd->set_en(1);
    lcd->delay_us(1);        /* Ancho minimo de pulso de Enable (>450ns) */
    lcd->set_en(0);
    lcd->delay_us(1);        /* Tiempo de recuperacion de Enable (>1us) */
}

/* -----------------------------------------------------------------
 * API publica
 * ----------------------------------------------------------------- */

void LCD_Command(LCD_HandleTypeDef *lcd, uint8_t cmd)
{
    lcd_send_byte(lcd, 0, cmd);

    /* Tiempos de ejecucion segun la tabla "Comandos basicos" */
    if (cmd == LCD_CMD_CLEAR_DISPLAY || cmd == LCD_CMD_RETURN_HOME) {
        lcd->delay_ms(2);       /* Estos dos comandos tardan ~1.64mS */
    } else {
        lcd->delay_us(45);      /* La mayoria de comandos tardan ~40uS */
    }
}

void LCD_Write(LCD_HandleTypeDef *lcd, uint8_t data)
{
    lcd_send_byte(lcd, 1, data);
    lcd->delay_us(45);
}

void LCD_Init(LCD_HandleTypeDef *lcd, uint8_t cols, uint8_t rows, uint8_t dotsize)
{
    lcd->_cols = cols;
    lcd->_rows = rows;

    lcd->_display_function = LCD_8BIT_MODE;
    lcd->_display_function |= (rows > 1) ? LCD_2LINE : LCD_1LINE;
    if (dotsize != 0 && rows == 1) {
        lcd->_display_function |= LCD_5x10DOTS;   /* 5x10 solo aplica a 1 linea */
    }

    /* -----------------------------------------------------------
     * Algoritmo de inicializacion para 8 bits (ver diapositiva
     * "Algoritmo de inicializacion para 8 bits").
     *
     * Aqui no hace falta el "triple intento" del modo 4 bits, porque
     * en modo 8 bits no hay riesgo de que el LCD interprete un
     * nibble alto como si fuera bajo o viceversa: basta esperar el
     * tiempo de encendido y mandar directamente el Function Set.
     * ----------------------------------------------------------- */

    lcd->set_rs(0);
    lcd->set_en(0);

    lcd->delay_ms(20);   /* "No esperar mas de 15mS" (20mS por margen) */

    LCD_Command(lcd, LCD_CMD_FUNCTION_SET | lcd->_display_function);
    lcd->delay_us(150);  /* Margen extra recomendado tras el primer comando */

    lcd->_display_control = LCD_DISPLAY_ON | LCD_CURSOR_OFF | LCD_BLINK_OFF;
    LCD_Command(lcd, LCD_CMD_DISPLAY_CONTROL | lcd->_display_control);

    LCD_Clear(lcd);

    lcd->_display_mode = LCD_ENTRY_LEFT | LCD_ENTRY_SHIFT_OFF;
    LCD_Command(lcd, LCD_CMD_ENTRY_MODE_SET | lcd->_display_mode);
}

void LCD_Clear(LCD_HandleTypeDef *lcd)
{
    LCD_Command(lcd, LCD_CMD_CLEAR_DISPLAY);
}

void LCD_Home(LCD_HandleTypeDef *lcd)
{
    LCD_Command(lcd, LCD_CMD_RETURN_HOME);
}

void LCD_SetCursor(LCD_HandleTypeDef *lcd, uint8_t col, uint8_t row)
{
    /* Direcciones de inicio de fila en la DDRAM (ver diapositiva
     * "Memoria DDRAM"). Soporta hasta 4 filas (16x4, 20x4, etc.) */
    static const uint8_t row_offsets[] = { 0x00, 0x40, 0x14, 0x54 };
    uint8_t max_rows = (uint8_t)(sizeof(row_offsets) / sizeof(row_offsets[0]));

    if (row >= max_rows)     row = max_rows - 1;
    if (row >= lcd->_rows)   row = lcd->_rows - 1;

    LCD_Command(lcd, LCD_CMD_SET_DDRAM_ADDR | (col + row_offsets[row]));
}

void LCD_Print(LCD_HandleTypeDef *lcd, const char *str)
{
    while (*str) {
        LCD_Write(lcd, (uint8_t)*str++);
    }
}

void LCD_PrintChar(LCD_HandleTypeDef *lcd, char c)
{
    LCD_Write(lcd, (uint8_t)c);
}

void LCD_CreateChar(LCD_HandleTypeDef *lcd, uint8_t location, const uint8_t charmap[8])
{
    uint8_t i;
    location &= 0x07;   /* Solo existen 8 localidades (0-7) en la CGRAM */

    LCD_Command(lcd, LCD_CMD_SET_CGRAM_ADDR | (location << 3));
    for (i = 0; i < 8; i++) {
        LCD_Write(lcd, charmap[i] & 0x1F);   /* Solo se usan los 5 bits bajos */
    }
    LCD_SetCursor(lcd, 0, 0);   /* Regresa el puntero de direccion a la DDRAM */
}

void LCD_Display(LCD_HandleTypeDef *lcd, uint8_t on)
{
    if (on) lcd->_display_control |= LCD_DISPLAY_ON;
    else    lcd->_display_control &= (uint8_t)~LCD_DISPLAY_ON;
    LCD_Command(lcd, LCD_CMD_DISPLAY_CONTROL | lcd->_display_control);
}

void LCD_Cursor(LCD_HandleTypeDef *lcd, uint8_t on)
{
    if (on) lcd->_display_control |= LCD_CURSOR_ON;
    else    lcd->_display_control &= (uint8_t)~LCD_CURSOR_ON;
    LCD_Command(lcd, LCD_CMD_DISPLAY_CONTROL | lcd->_display_control);
}

void LCD_Blink(LCD_HandleTypeDef *lcd, uint8_t on)
{
    if (on) lcd->_display_control |= LCD_BLINK_ON;
    else    lcd->_display_control &= (uint8_t)~LCD_BLINK_ON;
    LCD_Command(lcd, LCD_CMD_DISPLAY_CONTROL | lcd->_display_control);
}

void LCD_ScrollLeft(LCD_HandleTypeDef *lcd)
{
    LCD_Command(lcd, LCD_CMD_CURSOR_SHIFT | LCD_MOVE_DISPLAY | LCD_MOVE_LEFT);
}

void LCD_ScrollRight(LCD_HandleTypeDef *lcd)
{
    LCD_Command(lcd, LCD_CMD_CURSOR_SHIFT | LCD_MOVE_DISPLAY | LCD_MOVE_RIGHT);
}

void LCD_LeftToRight(LCD_HandleTypeDef *lcd)
{
    lcd->_display_mode |= LCD_ENTRY_LEFT;
    LCD_Command(lcd, LCD_CMD_ENTRY_MODE_SET | lcd->_display_mode);
}

void LCD_RightToLeft(LCD_HandleTypeDef *lcd)
{
    lcd->_display_mode &= (uint8_t)~LCD_ENTRY_LEFT;
    LCD_Command(lcd, LCD_CMD_ENTRY_MODE_SET | lcd->_display_mode);
}

void LCD_Autoscroll(LCD_HandleTypeDef *lcd, uint8_t on)
{
    if (on) lcd->_display_mode |= LCD_ENTRY_SHIFT_ON;
    else    lcd->_display_mode &= (uint8_t)~LCD_ENTRY_SHIFT_ON;
    LCD_Command(lcd, LCD_CMD_ENTRY_MODE_SET | lcd->_display_mode);
}