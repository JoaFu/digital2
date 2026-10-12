#ifndef GALAGA_DMA_H
#define GALAGA_DMA_H
#include <stdint.h>
void GD_Iniciar(void);
void GD_Enviar(const uint8_t *datos,uint16_t cantidad);
uint8_t GD_Ocupado(void);
void GD_Esperar(void); /* Solo transiciones de pantalla; no se usa por cuadro. */
extern volatile uint32_t galagaTransferenciasDMA,galagaErroresDMA;
#endif
