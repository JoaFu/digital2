#ifndef GALAGA_JUEGO_H
#define GALAGA_JUEGO_H
#include <stdint.h>

/* Motor independiente de HAL: paso fijo de 20 ms, sin malloc ni esperas. */
#define G_PASO_MS 20U
#define G_ANCHO 320
#define G_ALTO 240
#define G_HUD 20
#define G_ENEMIGOS 16
#define G_BALAS 24
#define G_PARTICULAS 64
#define G_HP_AMARILLO 2
#define G_HP_VERDE 3
#define G_HP_BOSS 20
#define G_CRUCE_MS 5000U
#define G_CRUCE_VUELO_MS 2200U
#define G_RAYO_CICLO_MS 4400U
#define G_RAYO_AVISO_MS 600U
#define G_RAYO_ACTIVO_MS 1400U
#define G_RAYO_ANCHO 48
#define G_RAYO_ALTO 80
#define G_RAYO_VELOCIDAD 4
#define G_BOSS_ENTRADA_MS 2200U
#define G_BOSS_TURNO_MS 900U

typedef enum { G_CLASICO, G_AMARILLO, G_VERDE, G_BOSS, G_CRUCE } GTipo;
typedef enum { G_JUGANDO, G_VICTORIA, G_DERROTA } GResultado;
typedef struct {
    int16_t x, y, origenX, destinoX, destinoY;
    uint8_t activo, tipo, hp, tam, entrando, fase, reposiciones;
    uint32_t edad, espera, disparo;
} GEnemigo;
typedef struct {
    int32_t x, y; /* Coordenadas en unidades de 1/256 pixel. */
    int16_t vx, vy;
    uint8_t activa;
} GBala;
typedef struct {
    int16_t x, y, vx, vy;
    uint16_t color;
    uint8_t vida;
} GParticula;
typedef struct {
    uint8_t izquierda, derecha, disparar;
} GEntrada;
typedef struct {
    GEnemigo enemigos[G_ENEMIGOS];
    GBala balas[G_BALAS], jugador;
    GParticula particulas[G_PARTICULAS];
    int16_t naveX, naveY, rayoX, rayoY;
    uint8_t nivel, fase, resultado, rayo, lento, reposicionUsada;
    uint16_t bajas, objetivo, protecciones, lentoRestante, celebracion;
    uint32_t tiempo, cruce, cicloRayo, semilla;
    uint16_t esperaAtaque;
    uint8_t turnoAtaque; /* 0..3: escoltas por posicion inicial; 4: jefe. */
} GJuego;

void G_Iniciar(GJuego *g, uint8_t nivel);
void G_Paso(GJuego *g, GEntrada entrada);
uint8_t G_ObjetivosVivos(const GJuego *g);
#endif
