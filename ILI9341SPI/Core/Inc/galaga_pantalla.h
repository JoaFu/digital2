#ifndef GALAGA_PANTALLA_H
#define GALAGA_PANTALLA_H
#include "galaga_juego.h"
void GP_Iniciar(void);
void GP_Portada(void);
void GP_Menu(uint8_t seleccionado);
void GP_Resultado(uint8_t victoria);
void GP_PrepararJuego(void);
void GP_Marcar(const GJuego *g);
void GP_Dibujar(const GJuego *g);
/* Medicion real, visible en debugger: coste y bytes del ultimo dibujo. */
extern volatile uint32_t galagaDibujoMs, galagaBytesFrame, galagaFrameMaxMs;
#endif
