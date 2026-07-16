/* =====================================================================
 * lcd_hd44780_4bits.h
 * ---------------------------------------------------------------------
 * Libreria generica y portable para LCD basado en el controlador
 * HD44780, operando en modo de 4 BITS (se envia primero el nibble
 * alto y luego el nibble bajo de cada byte, por las lineas D4-D7).
 *
 * Esta libreria NO esta atada a ningun microcontrolador en particular.
 * El usuario debe implementar 5 funciones "callback" que controlan
 * los pines fisicos (RS, EN, D4-D7) y los delays de su plataforma
 * (Arduino, PIC, AVR, STM32, MSP430, etc.), y colocarlas dentro de una
 * estructura LCD_HandleTypeDef antes de llamar a LCD_Init().
 *
 * IE3054 - Electronica Digital 2
 * ===================================================================== */

#ifndef LCD_HD44780_4BITS_H
#define LCD_HD44780_4BITS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------
 * Comandos basicos (ver tabla "Comandos basicos" del curso)
 * --------------------------------------------------------------------- */
#define LCD_CMD_CLEAR_DISPLAY     0x01
#define LCD_CMD_RETURN_HOME       0x02
#define LCD_CMD_ENTRY_MODE_SET    0x04
#define LCD_CMD_DISPLAY_CONTROL   0x08
#define LCD_CMD_CURSOR_SHIFT      0x10
#define LCD_CMD_FUNCTION_SET      0x20
#define LCD_CMD_SET_CGRAM_ADDR    0x40
#define LCD_CMD_SET_DDRAM_ADDR    0x80

/* Flags para LCD_CMD_ENTRY_MODE_SET (I/D, S) */
#define LCD_ENTRY_LEFT             0x02   /* I/D = 1 -> incrementa direccion */
#define LCD_ENTRY_RIGHT            0x00   /* I/D = 0 -> decrementa direccion */
#define LCD_ENTRY_SHIFT_ON         0x01   /* S = 1   -> desplaza pantalla   */
#define LCD_ENTRY_SHIFT_OFF        0x00

/* Flags para LCD_CMD_DISPLAY_CONTROL (D, U, B) */
#define LCD_DISPLAY_ON             0x04
#define LCD_DISPLAY_OFF            0x00
#define LCD_CURSOR_ON              0x02
#define LCD_CURSOR_OFF             0x00
#define LCD_BLINK_ON               0x01
#define LCD_BLINK_OFF              0x00

/* Flags para LCD_CMD_CURSOR_SHIFT (D/C, R/L) */
#define LCD_MOVE_DISPLAY           0x08
#define LCD_MOVE_CURSOR            0x00
#define LCD_MOVE_RIGHT             0x04
#define LCD_MOVE_LEFT              0x00

/* Flags para LCD_CMD_FUNCTION_SET (DL, N, F) */
#define LCD_8BIT_MODE              0x10
#define LCD_4BIT_MODE              0x00
#define LCD_2LINE                  0x08
#define LCD_1LINE                  0x00
#define LCD_5x10DOTS               0x04
#define LCD_5x8DOTS                0x00

/* ---------------------------------------------------------------------
 * Estructura de manejo (handle) del LCD.
 *
 * El usuario debe llenar los 5 punteros a funcion con su propia
 * implementacion para el microcontrolador que este usando. Esto es lo
 * que hace que la libreria sea portable: el .c de la libreria nunca
 * toca registros de hardware directamente.
 * --------------------------------------------------------------------- */
typedef struct {

    /* Pone en alto (1) o bajo (0) el pin RS (Register Select) */
    void (*set_rs)(uint8_t state);

    /* Pone en alto (1) o bajo (0) el pin E (Enable) */
    void (*set_en)(uint8_t state);

    /* Escribe un nibble (4 bits, en los 4 bits menos significativos
     * del parametro) en las lineas D4-D7 del LCD */
    void (*write_nibble)(uint8_t nibble);

    /* Delay en microsegundos */
    void (*delay_us)(uint16_t us);

    /* Delay en milisegundos */
    void (*delay_ms)(uint16_t ms);

    /* --- Campos internos de estado, no modificar manualmente --- */
    uint8_t _display_function;
    uint8_t _display_control;
    uint8_t _display_mode;
    uint8_t _cols;
    uint8_t _rows;

} LCD_HandleTypeDef;

/* ---------------------------------------------------------------------
 * API publica
 * --------------------------------------------------------------------- */

/* Inicializa el LCD siguiendo el algoritmo de inicializacion de 4 bits.
 * cols, rows : dimensiones del LCD (ej. 16,2 ; 20,4 ; 16,1 ...)
 * dotsize    : LCD_5x8DOTS o LCD_5x10DOTS (5x10 solo valido si rows == 1) */
void LCD_Init(LCD_HandleTypeDef *lcd, uint8_t cols, uint8_t rows, uint8_t dotsize);

/* Envia un byte de COMANDO (RS = 0). Se encarga de partirlo en 2 nibbles. */
void LCD_Command(LCD_HandleTypeDef *lcd, uint8_t cmd);

/* Envia un byte de DATO (RS = 1), es decir, un caracter a la DDRAM/CGRAM */
void LCD_Write(LCD_HandleTypeDef *lcd, uint8_t data);

/* Borra la pantalla y regresa el cursor a la posicion inicial (0,0) */
void LCD_Clear(LCD_HandleTypeDef *lcd);

/* Regresa el cursor a la posicion inicial sin borrar el contenido */
void LCD_Home(LCD_HandleTypeDef *lcd);

/* Ubica el cursor en la columna y fila indicadas (base 0) */
void LCD_SetCursor(LCD_HandleTypeDef *lcd, uint8_t col, uint8_t row);

/* Imprime una cadena de caracteres terminada en '\0' */
void LCD_Print(LCD_HandleTypeDef *lcd, const char *str);

/* Imprime un solo caracter */
void LCD_PrintChar(LCD_HandleTypeDef *lcd, char c);

/* Crea un caracter propio en la CGRAM (location: 0-7). charmap: 8 bytes,
 * los 5 bits menos significativos de cada byte definen una fila del
 * caracter (formato 5x8), de arriba hacia abajo */
void LCD_CreateChar(LCD_HandleTypeDef *lcd, uint8_t location, const uint8_t charmap[8]);

/* Enciende / apaga el visualizador (sin perder el contenido de la DDRAM) */
void LCD_Display(LCD_HandleTypeDef *lcd, uint8_t on);

/* Muestra / oculta el cursor */
void LCD_Cursor(LCD_HandleTypeDef *lcd, uint8_t on);

/* Activa / desactiva el parpadeo del cursor */
void LCD_Blink(LCD_HandleTypeDef *lcd, uint8_t on);

/* Desplaza todo el contenido visible de la pantalla una posicion */
void LCD_ScrollLeft(LCD_HandleTypeDef *lcd);
void LCD_ScrollRight(LCD_HandleTypeDef *lcd);

/* Define la direccion en que se escriben los caracteres nuevos */
void LCD_LeftToRight(LCD_HandleTypeDef *lcd);
void LCD_RightToLeft(LCD_HandleTypeDef *lcd);

/* Activa / desactiva el autoscroll (bit S del modo de entrada) */
void LCD_Autoscroll(LCD_HandleTypeDef *lcd, uint8_t on);

#ifdef __cplusplus
}
#endif

#endif /* LCD_HD44780_4BITS_H */