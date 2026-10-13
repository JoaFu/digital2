#ifndef GALAGA_RECORDS_H
#define GALAGA_RECORDS_H
#include <stdint.h>

void GR_Iniciar(void);
void GR_Comprobar(void);
uint8_t GR_Disponible(void);
uint32_t GR_Mejor(uint8_t nivel);
// Devuelve 1 solamente despues de escribir, cerrar y releer la SD.
uint8_t GR_GuardarVictoria(uint8_t nivel, uint32_t milisegundos);
extern volatile uint32_t galagaRecordError;
#endif
