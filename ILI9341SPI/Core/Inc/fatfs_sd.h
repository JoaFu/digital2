#ifndef FATFS_SD_H
#define FATFS_SD_H
#include <stdint.h>
// Limite global para una operacion de archivos, ademas de los limites SPI.
void SD_Operacion(uint32_t limiteMs);
extern volatile uint32_t galagaSDError;
#endif
