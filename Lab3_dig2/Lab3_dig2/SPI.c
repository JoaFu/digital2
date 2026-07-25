/* =========================================================
 *  spi.c - Libreria propia de SPI para ATmega328P
 *  Uso academico - Laboratorio 3: SPI
 * ========================================================= */

#include "SPI.h"

/* =========================================================
 *  Tabla de traduccion de prescaler "humano" -> bits reales.
 *  Cada fila = { SPR1, SPR0, SPI2X } para un divisor dado.
 *  Indice del arreglo = valor del enum spi_clock_t.
 * ========================================================= */
static const uint8_t spi_prescaler_table[7][3] =
{
    /* SPR1  SPR0  SPI2X */
    {   0,    0,    1   },   /* SPI_CLOCK_DIV2   */
    {   0,    0,    0   },   /* SPI_CLOCK_DIV4   */
    {   0,    1,    1   },   /* SPI_CLOCK_DIV8   */
    {   0,    1,    0   },   /* SPI_CLOCK_DIV16  */
    {   1,    0,    1   },   /* SPI_CLOCK_DIV32  */
    {   1,    0,    0   },   /* SPI_CLOCK_DIV64  */
    {   1,    1,    0   },   /* SPI_CLOCK_DIV128 */
};

/* =========================================================
 *  SPI_Init
 *  Configura pines y registros SPCR/SPSR segun "config".
 * ========================================================= */
void SPI_Init(spi_config_t config)
{
    if (config.role == SPI_ROLE_MASTER)
    {
        /* SS, MOSI y SCK como salidas. MISO como entrada */
        SPI_DDR |= (1 << SPI_SS) | (1 << SPI_MOSI) | (1 << SPI_SCK);
        SPI_DDR &= ~(1 << SPI_MISO);

        /* SS en alto: por defecto no se selecciona ningun esclavo */
        SPI_PORT |= (1 << SPI_SS);
    }
    else /* SPI_ROLE_SLAVE */
    {
        /* MISO como salida */
        SPI_DDR |= (1 << SPI_MISO);

        /* SS, MOSI y SCK como entradas (los controla el maestro) */
        SPI_DDR &= ~((1 << SPI_SS) | (1 << SPI_MOSI) | (1 << SPI_SCK));
    }

    /* ---------- Armado del registro SPCR ---------- */
    uint8_t spcr = (1 << SPE); /* siempre se habilita el modulo SPI */

    if (config.role == SPI_ROLE_MASTER)
    {
        spcr |= (1 << MSTR);
    }

    if (config.data_order == SPI_DATA_ORDER_LSB_FIRST)
    {
        spcr |= (1 << DORD);
    }

    /* CPOL = bit alto de clock_mode, CPHA = bit bajo de clock_mode */
    if (config.clock_mode & 0b10)
    {
        spcr |= (1 << CPOL);
    }
    if (config.clock_mode & 0b01)
    {
        spcr |= (1 << CPHA);
    }

    /* El prescaler solo tiene sentido en modo maestro */
    uint8_t spi2x = 0;
    if (config.role == SPI_ROLE_MASTER)
    {
        uint8_t spr1  = spi_prescaler_table[config.clock_div][0];
        uint8_t spr0  = spi_prescaler_table[config.clock_div][1];
        spi2x         = spi_prescaler_table[config.clock_div][2];

        spcr |= (spr1 << SPR1) | (spr0 << SPR0);
    }

    SPCR = spcr;
    SPSR = (spi2x << SPI2X); /* limpia flags y setea (o no) doble velocidad */
}

/* =========================================================
 *  SPI_MasterTransceive
 *  Envia "data" por MOSI y retorna lo que llega por MISO.
 *
 *  Pasos:
 *   1. Se carga el byte en SPDR -> esto dispara la transmision
 *   2. Se espera a que el flag SPIF se ponga en 1 (fin de envio)
 *   3. Se retorna lo que quedo en SPDR (dato recibido)
 * ========================================================= */
uint8_t SPI_MasterTransceive(uint8_t data)
{
    SPDR = data;

    while (!(SPSR & (1 << SPIF)))
        ; /* espera activa a que termine la transmision (sin delay) */

    return SPDR;
}

/* =========================================================
 *  SPI_SlaveReceive
 *  Bloquea hasta que el maestro complete una transaccion SPI
 *  y retorna el byte recibido en SPDR.
 * ========================================================= */
uint8_t SPI_SlaveReceive(void)
{
    while (!(SPSR & (1 << SPIF)))
        ; /* espera a que el maestro envie un byte */

    return SPDR;
}

/* =========================================================
 *  SPI_SlaveLoadData
 *  Prepara el byte que el esclavo respondera al maestro en la
 *  PROXIMA transaccion (el envio real ocurre cuando el maestro
 *  genera los pulsos de reloj).
 * ========================================================= */
void SPI_SlaveLoadData(uint8_t data)
{
    SPDR = data;
}