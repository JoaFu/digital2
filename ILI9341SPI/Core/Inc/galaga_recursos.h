#ifndef GALAGA_RECURSOS_H
#define GALAGA_RECURSOS_H

#include <stdint.h>

/* Sprites importados como RGB565 uint16_t, byte alto primero.
   Negro (0x0000) es transparente; los otros colores se preservan. */
#define GALAGA_SPRITES_NUEVOS 1
#define GALAGA_SPRITE_RAYO 1
#if GALAGA_SPRITES_NUEVOS
extern const uint16_t enemigo_amarillo[16 * 16];
extern const uint16_t enemigo_verde[16 * 16];
extern const uint16_t enemigo_boss[16 * 16];
#endif
#if GALAGA_SPRITE_RAYO
extern const uint16_t rayo_azul[48 * 80];
#endif

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

// Melodias de resultado: se reproducen una sola vez.
#define CANTIDAD_NOTAS_GAMEOVER 17
extern const NotaGalaga melodiaGameOver[CANTIDAD_NOTAS_GAMEOVER];
#define CANTIDAD_NOTAS_VICTORIA 9
extern const NotaGalaga melodiaVictoria[CANTIDAD_NOTAS_VICTORIA];

#endif
