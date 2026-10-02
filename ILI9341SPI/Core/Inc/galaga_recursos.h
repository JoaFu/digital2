#ifndef GALAGA_RECURSOS_H
#define GALAGA_RECURSOS_H

#include <stdint.h>

// Dimensiones y cantidades de los recursos del juego.
#define GALAGA_PANTALLA_ANCHO 320
#define GALAGA_PANTALLA_ALTO  240
#define GALAGA_SPRITE_LADO    16
#define CANTIDAD_ESTRELLAS    44
#define CANTIDAD_NOTAS_GALAGA 34

typedef struct
{
	uint16_t x;
	uint16_t y;
	uint16_t color;
	uint8_t tamano;
	uint8_t velocidad;
} Estrella;

typedef struct
{
    uint16_t frecuencia;
    uint16_t duracion;
} NotaGalaga;

// Imagen y sprites originales: datos constantes almacenados en Flash.
extern const uint16_t pantallaInicio[GALAGA_PANTALLA_ANCHO * GALAGA_PANTALLA_ALTO];
extern const uint8_t naveBytes[GALAGA_SPRITE_LADO * GALAGA_SPRITE_LADO * 2];
extern const uint8_t enemigoSmllBytes[GALAGA_SPRITE_LADO * GALAGA_SPRITE_LADO * 2];
extern const uint8_t balaBytes[GALAGA_SPRITE_LADO * GALAGA_SPRITE_LADO * 2];
extern const NotaGalaga melodiaGalaga[CANTIDAD_NOTAS_GALAGA];

// Arreglos modificables en RAM: estrellas y sprites convertidos al iniciar.
extern Estrella estrellas[CANTIDAD_ESTRELLAS];
extern uint16_t nave[GALAGA_SPRITE_LADO * GALAGA_SPRITE_LADO];
extern uint16_t enemigoPec[GALAGA_SPRITE_LADO * GALAGA_SPRITE_LADO];
extern uint16_t bala[GALAGA_SPRITE_LADO * GALAGA_SPRITE_LADO];

#endif
