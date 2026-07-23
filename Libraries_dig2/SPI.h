/* 
 *  spi.h - Libreria propia de SPI para ATmega328P
 *  Uso academico - Laboratorio 3: SPI
 *
 *  Pines SPI en el ATmega328P (Arduino Nano):
 *      PB2 (D10) -> SS   (Slave Select)
 *      PB3 (D11) -> MOSI (Master Out Slave In)
 *      PB4 (D12) -> MISO (Master In Slave Out)
 *      PB5 (D13) -> SCK  (Serial Clock)
 *
 *  NOTA DE HARDWARE:
 *  Los bits SPR1:SPR0 (en SPCR) solo dan 4 divisores (4,16,64,128).
 *  Los divisores restantes (2,8,32) se logran activando el bit
 *  SPI2X, que esta en OTRO registro: SPSR (no en SPCR).
 *  Por eso el prescaler se maneja aqui como un enum separado
 *  y es la funcion SPI_Init() la que arma correctamente
 *  SPCR y SPSR a partir de la configuracion.
 * ========================================================= */

#ifndef SPI_H
#define SPI_H

#include <avr/io.h>

/* ---------- Definicion de pines (Puerto B) ---------- */
#define SPI_DDR   DDRB
#define SPI_PORT  PORTB
#define SPI_SS    PB2
#define SPI_MOSI  PB3
#define SPI_MISO  PB4
#define SPI_SCK   PB5

/* ---------------------------------------------------------
 *  Rol del dispositivo dentro del bus SPI
 * --------------------------------------------------------- */
typedef enum
{
    SPI_ROLE_MASTER = 0,
    SPI_ROLE_SLAVE  = 1
} spi_role_t;

/* ---------------------------------------------------------
 *  Orden en que se transmiten los bits del byte
 * --------------------------------------------------------- */
typedef enum
{
    SPI_DATA_ORDER_MSB_FIRST = 0,   /* bit mas significativo primero (default) */
    SPI_DATA_ORDER_LSB_FIRST = 1    /* bit menos significativo primero */
} spi_data_order_t;

/* ---------------------------------------------------------
 *  Modo de reloj SPI (combinacion de CPOL y CPHA)
 *  MODE0: CPOL=0, CPHA=0 -> el mas usado (default)
 *  MODE1: CPOL=0, CPHA=1
 *  MODE2: CPOL=1, CPHA=0
 *  MODE3: CPOL=1, CPHA=1
 * --------------------------------------------------------- */
typedef enum
{
    SPI_MODE0 = 0,
    SPI_MODE1 = 1,
    SPI_MODE2 = 2,
    SPI_MODE3 = 3
} spi_clock_mode_t;

/* ---------------------------------------------------------
 *  Prescaler del reloj SPI (solo aplica si role = MASTER).
 *  Nombrado segun el divisor real de Fosc.
 *  A 16MHz, DIV16 -> 1MHz de reloj SPI, por ejemplo.
 * --------------------------------------------------------- */
typedef enum
{
    SPI_CLOCK_DIV2   = 0,
    SPI_CLOCK_DIV4   = 1,
    SPI_CLOCK_DIV8   = 2,
    SPI_CLOCK_DIV16  = 3,
    SPI_CLOCK_DIV32  = 4,
    SPI_CLOCK_DIV64  = 5,
    SPI_CLOCK_DIV128 = 6
} spi_clock_t;

/* ---------------------------------------------------------
 *  Estructura de configuracion completa del periferico SPI.
 *  Se llena y se pasa una sola vez a SPI_Init().
 * --------------------------------------------------------- */
typedef struct
{
    spi_role_t       role;         /* SPI_ROLE_MASTER o SPI_ROLE_SLAVE   */
    spi_data_order_t data_order;   /* MSB_FIRST o LSB_FIRST              */
    spi_clock_mode_t clock_mode;   /* MODE0 - MODE3                      */
    spi_clock_t      clock_div;    /* Solo se usa si role = MASTER       */
} spi_config_t;

/* ---------------------------------------------------------
 *  SPI_Init
 *  Configura pines y registros (SPCR/SPSR) segun "config".
 *  Ejemplo de uso (maestro):
 *
 *      spi_config_t cfg = {
 *          .role       = SPI_ROLE_MASTER,
 *          .data_order = SPI_DATA_ORDER_MSB_FIRST,
 *          .clock_mode = SPI_MODE0,
 *          .clock_div  = SPI_CLOCK_DIV16
 *      };
 *      SPI_Init(cfg);
 *
 *  Ejemplo de uso (esclavo, clock_div se ignora):
 *
 *      spi_config_t cfg = {
 *          .role       = SPI_ROLE_SLAVE,
 *          .data_order = SPI_DATA_ORDER_MSB_FIRST,
 *          .clock_mode = SPI_MODE0,
 *          .clock_div  = SPI_CLOCK_DIV16   // no se usa, pero hay que llenarlo
 *      };
 *      SPI_Init(cfg);
 * --------------------------------------------------------- */
void SPI_Init(spi_config_t config);

/* ---------------------------------------------------------
 *  MAESTRO: envia un byte y retorna el byte recibido
 *  (SPI es full-duplex, siempre se recibe algo al transmitir)
 * --------------------------------------------------------- */
uint8_t SPI_MasterTransceive(uint8_t data);

/* ---------------------------------------------------------
 *  ESCLAVO: espera a que el maestro envie un byte y lo retorna
 *  (funcion bloqueante, se queda esperando el flag SPIF)
 * --------------------------------------------------------- */
uint8_t SPI_SlaveReceive(void);

/* ---------------------------------------------------------
 *  ESCLAVO: carga un byte en el registro de datos para que
 *  sea enviado al maestro durante la SIGUIENTE transaccion.
 *  (no bloquea, solo prepara el dato)
 * --------------------------------------------------------- */
void SPI_SlaveLoadData(uint8_t data);

#endif /* SPI_H */