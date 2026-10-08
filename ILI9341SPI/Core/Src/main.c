/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ili9341.h"
#include "bitmaps.h"
#include "galaga_recursos.h"
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

typedef enum
{
    ESTADO_INICIO,
    ESTADO_NIVEL_1,
    ESTADO_NIVEL_2,
    ESTADO_GAMEOVER,
    ESTADO_YOU_WIN
} EstadoJuego;

typedef struct
{
    int x, y;
    int direccion;
    int limiteIzquierdo, limiteDerecho;
    uint8_t vivo;
    uint32_t ultimoDisparo;
} Enemigo;

typedef struct
{
    int x, y;
    uint8_t activa;
} Proyectil;

typedef struct
{
    uint8_t crudo, estable;
    uint32_t ultimoCambio;
} BotonFiltrado;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define SCREEN_WIDTH             320
#define SCREEN_HEIGHT            240

#define SPRITE_SIZE              16
// Ambas balas vuelven al tamano original de 16 x 16.
#define BALA_ESCALA              1
#define BALA_TAMANO              (SPRITE_SIZE * BALA_ESCALA)

// Pantalla horizontal: la nave (abajo) y el enemigo (arriba) solo se mueven
// de izquierda a derecha; la bala sube.
#define NAVE_X_INICIAL           152
#define NAVE_Y_INICIAL           216

#define ENEMIGO_X_INICIAL        152
#define ENEMIGO_Y_INICIAL        32

#define NAVE_VELOCIDAD           2
#define ENEMIGO_VELOCIDAD        2
#define BALA_VELOCIDAD           5

#define MARCADOR_ALTO            (fontYSizeSmal + 8)

#define LIMITE_IZQUIERDO         0
#define LIMITE_DERECHO           (SCREEN_WIDTH - SPRITE_SIZE)

// Botones de movimiento. Si izquierda y derecha te quedan al reves,
// intercambia estas cuatro lineas.
#define BTN_IZQUIERDA_PORT       BTN_UP_GPIO_Port
#define BTN_IZQUIERDA_PIN        BTN_UP_Pin
#define BTN_DERECHA_PORT         BTN_DOWN_GPIO_Port
#define BTN_DERECHA_PIN          BTN_DOWN_Pin

#define COLOR_FONDO              0x0000

// B1 esta en alto en reposo y en bajo al pulsarlo.
#define B1_PRESIONADO             GPIO_PIN_RESET

#define FRAME_DELAY_MS           20
#define ESTRELLAS_DELAY_MS       100
// Parametros del esqueleto: una vida, tres enemigos en nivel 2.
#define MAX_ENEMIGOS             3
#define BALA_ENEMIGA_VELOCIDAD    3
#define DISPARO_ENEMIGO_MS        1200U
#define DESFASE_DISPARO_MS        250U
#define RESULTADO_MS              2500U
#define ANTIRREBOTE_MS            30U

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef hspi1;

TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim7;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */

static EstadoJuego estadoJuego = ESTADO_INICIO;
static uint32_t tiempoEstado = 0;
static uint32_t tiempoFrameAnterior = 0;
static uint32_t tiempoEstrellasAnterior = 0;
static uint32_t contador = 0;

// Estado de la partida. Los datos graficos siguen en galaga_recursos.c.
static Enemigo enemigos[MAX_ENEMIGOS];
static Proyectil balasEnemigas[MAX_ENEMIGOS];
static Proyectil balaJugador;
static uint8_t cantidadEnemigos = 0;
static int naveX = NAVE_X_INICIAL;
static int naveY = NAVE_Y_INICIAL;

static BotonFiltrado botonB1;
static uint8_t inicioArmado = 0;
static uint8_t inicioSolicitado = 0;
static uint8_t disparoPendiente = 0;

static uint32_t notaActual = 0;
static uint32_t inicioNota = 0;
static uint8_t melodiaEnCurso = 0;
static const NotaGalaga *melodiaActual = melodiaGalaga;
static uint32_t cantidadNotasActual = CANTIDAD_NOTAS_GALAGA;
static uint8_t repetirMelodia = 1;

// Memoria temporal de transmision; los arreglos de imagen siguen en la libreria.
static uint8_t bufferLCD[512];

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_SPI1_Init(void);
static void MX_TIM3_Init(void);
static void MX_TIM7_Init(void);
/* USER CODE BEGIN PFP */

void DibujarPantallaInicio(void);
void DibujarContador(void);
void DibujarFondoEstrellas(void);
void ActualizarEstrellas(void);
void RestaurarFondoEstrellas(int x, int y, int ancho, int alto);
void ConvertirSpriteRGB565(const uint8_t *bytes, uint16_t *sprite, int cantidadPixeles);
void Audio_Iniciar(void);
void Audio_Detener(void);
void Audio_Actualizar(void);
static void Audio_TocarNota(uint16_t frecuencia);
static void Audio_Reproducir(const NotaGalaga *notas, uint32_t cantidad, uint8_t repetir);

static void FSM_CambiarEstado(EstadoJuego nuevo);
static void FSM_Actualizar(void);
static void PrepararNivel(void);
static void ActualizarPartida(uint32_t ahora);
static void Pantalla_Enviar(uint8_t *datos, uint16_t cantidad);
static void Pantalla_Abrir(int x, int y, int ancho, int alto);
static void Pantalla_Rellenar(int x, int y, int ancho, int alto, uint16_t color);
static void Pantalla_Bitmap(int x, int y, int ancho, int alto,
                            const uint16_t *imagen, uint8_t invertir, int minimoY);
static void BorrarActores(void);
static void DibujarActores(void);
static void DibujarProyectil(const Proyectil *p, uint8_t haciaAbajo);
static void BorrarProyectil(const Proyectil *p);
static uint8_t ActualizarB1(uint32_t ahora);
static uint8_t Impacta(const Proyectil *p, int x, int y);
static uint8_t EnemigosVivos(void);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

// Transferencias en bloques: evita llamar a HAL una vez por cada byte RGB565.
// Buffer compartido de 512 bytes; estas funciones solo se usan desde main.
static void Pantalla_Enviar(uint8_t *datos, uint16_t cantidad)
{
    if (HAL_SPI_Transmit(&hspi1, datos, cantidad, 100U) != HAL_OK)
    {
        HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
        Error_Handler();
    }
}

static void Pantalla_Abrir(int x, int y, int ancho, int alto)
{
    uint8_t comando;
    uint8_t limites[4];
    uint16_t finX = (uint16_t)(x + ancho - 1);
    uint16_t finY = (uint16_t)(y + alto - 1);
    HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_RESET);
    comando = 0x2A;
    Pantalla_Enviar(&comando, 1U);
    HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_SET);
    limites[0] = (uint8_t)(x >> 8);
    limites[1] = (uint8_t)x;
    limites[2] = (uint8_t)(finX >> 8);
    limites[3] = (uint8_t)finX;
    Pantalla_Enviar(limites, 4U);
    HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_RESET);
    comando = 0x2B;
    Pantalla_Enviar(&comando, 1U);
    HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_SET);
    limites[0] = (uint8_t)(y >> 8);
    limites[1] = (uint8_t)y;
    limites[2] = (uint8_t)(finY >> 8);
    limites[3] = (uint8_t)finY;
    Pantalla_Enviar(limites, 4U);
    HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_RESET);
    comando = 0x2C;
    Pantalla_Enviar(&comando, 1U);
    HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_SET);
}

static void Pantalla_Rellenar(int x, int y, int ancho, int alto, uint16_t color)
{
    int x2 = x + ancho;
    int y2 = y + alto;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x2 > SCREEN_WIDTH) x2 = SCREEN_WIDTH;
    if (y2 > SCREEN_HEIGHT) y2 = SCREEN_HEIGHT;
    if (x >= x2 || y >= y2) return;

    uint32_t restantes = (uint32_t)(x2 - x) * (uint32_t)(y2 - y);
    uint32_t bloque = restantes < sizeof(bufferLCD) / 2U ? restantes : sizeof(bufferLCD) / 2U;
    for (uint32_t i = 0; i < bloque; i++)
    {
        bufferLCD[2U * i] = (uint8_t)(color >> 8);
        bufferLCD[2U * i + 1U] = (uint8_t)color;
    }
    Pantalla_Abrir(x, y, x2 - x, y2 - y);
    while (restantes > 0U)
    {
        uint32_t cantidad = restantes < bloque ? restantes : bloque;
        Pantalla_Enviar(bufferLCD, (uint16_t)(cantidad * 2U));
        restantes -= cantidad;
    }
    HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
}

static void Pantalla_Bitmap(int x, int y, int ancho, int alto,
                            const uint16_t *imagen, uint8_t invertir, int minimoY)
{
    int x1 = x < 0 ? 0 : x;
    int y1 = y < minimoY ? minimoY : y;
    int x2 = x + ancho;
    int y2 = y + alto;
    if (x2 > SCREEN_WIDTH) x2 = SCREEN_WIDTH;
    if (y2 > SCREEN_HEIGHT) y2 = SCREEN_HEIGHT;
    if (x1 >= x2 || y1 >= y2) return;

    uint16_t usados = 0;
    Pantalla_Abrir(x1, y1, x2 - x1, y2 - y1);
    for (int destinoY = y1; destinoY < y2; destinoY++)
    {
        int fila = destinoY - y;
        if (invertir) fila = alto - 1 - fila;
        const uint16_t *origen = imagen + fila * ancho + (x1 - x);
        for (int columna = x1; columna < x2; columna++)
        {
            uint16_t pixel = *origen++;
            bufferLCD[usados++] = (uint8_t)(pixel >> 8);
            bufferLCD[usados++] = (uint8_t)pixel;
            if (usados == sizeof(bufferLCD))
            {
                Pantalla_Enviar(bufferLCD, usados);
                usados = 0;
            }
        }
    }
    if (usados != 0U) Pantalla_Enviar(bufferLCD, usados);
    HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
}

void DibujarPantallaInicio(void)
{
    // Lee directamente desde Flash y envia cada pixel al LCD.
    Pantalla_Bitmap(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, pantallaInicio, 0U, 0);
}

void DibujarContador(void)
{
    char texto[40];
    unsigned int nivel = (estadoJuego == ESTADO_NIVEL_1) ? 1U : 2U;
    snprintf(texto, sizeof(texto), "NIVEL %u  BAJAS: %lu/4", nivel, (unsigned long)contador);
    Pantalla_Rellenar(0, 0, SCREEN_WIDTH, MARCADOR_ALTO, COLOR_FONDO);
    LCD_Print(texto, 8, 4, 1, 0xFFFF, COLOR_FONDO);
}
void DibujarFondoEstrellas(void)
{
	Pantalla_Rellenar(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_FONDO);

	for (uint32_t i = 0; i < CANTIDAD_ESTRELLAS; i++)
	{
        // Las estrellas no dibujan ni borran dentro del marcador.
        if (estrellas[i].y < MARCADOR_ALTO)
        {
            continue;
        }

		Pantalla_Rellenar(estrellas[i].x,
				 estrellas[i].y,
				 estrellas[i].tamano,
				 estrellas[i].tamano,
				 estrellas[i].color);
	}
}

void ActualizarEstrellas(void)
{
	uint32_t ahora = HAL_GetTick();

	if ((ahora - tiempoEstrellasAnterior) < ESTRELLAS_DELAY_MS)
	{
		return;
	}

	tiempoEstrellasAnterior = ahora;

	for (uint32_t i = 0; i < CANTIDAD_ESTRELLAS; i++)
	{
		Estrella *e = &estrellas[i];

		// Borra la posicion anterior (nada se dibuja dentro del marcador)
		if (e->y >= MARCADOR_ALTO)
		{
			Pantalla_Rellenar(e->x, e->y, e->tamano, e->tamano, COLOR_FONDO);
		}

		// Movimiento hacia abajo; al salir reaparece arriba, bajo el marcador
		if (e->y + e->velocidad + e->tamano > SCREEN_HEIGHT)
		{
			e->y = MARCADOR_ALTO;
		}
		else
		{
			e->y += e->velocidad;
		}

		// Dibuja la estrella en su nueva posicion
		if (e->y >= MARCADOR_ALTO)
		{
			Pantalla_Rellenar(e->x, e->y, e->tamano, e->tamano, e->color);
		}
	}
}

void RestaurarFondoEstrellas(int x,
							 int y,
							 int ancho,
							 int alto)
{
	Pantalla_Rellenar(x, y, ancho, alto, COLOR_FONDO);

	for (uint32_t i = 0; i < CANTIDAD_ESTRELLAS; i++)
	{
        // Las estrellas no dibujan ni borran dentro del marcador.
        if (estrellas[i].y < MARCADOR_ALTO)
        {
            continue;
        }

		int estrellaX = estrellas[i].x;
		int estrellaY = estrellas[i].y;
		int estrellaTamano = estrellas[i].tamano;

		if ((estrellaX < x + ancho) &&
			(estrellaX + estrellaTamano > x) &&
			(estrellaY < y + alto) &&
			(estrellaY + estrellaTamano > y))
		{
			Pantalla_Rellenar(estrellas[i].x,
					 estrellas[i].y,
					 estrellas[i].tamano,
					 estrellas[i].tamano,
					 estrellas[i].color);
		}
	}
}

void ConvertirSpriteRGB565(const uint8_t *bytes,
						   uint16_t *sprite,
						   int cantidadPixeles)
{
	for (int i = 0; i < cantidadPixeles; i++)
	{
		sprite[i] = ((uint16_t)bytes[i * 2] << 8) |
					bytes[i * 2 + 1];
	}
}

static void Audio_TocarNota(uint16_t frecuencia)
{
    // TIM3 recibe 84 MHz: PSC=83 produce un contador de 1 MHz.
    // Durante las pausas se mantiene el temporizador con CCR1=0.
    uint32_t periodo = 1000U;
    uint32_t pulso = 0U;

    if (frecuencia != 0U)
    {
        periodo = (1000000U + frecuencia / 2U) / frecuencia;
        pulso = periodo / 2U;
    }

    // Carga ARR y CCR1 juntos para evitar un ciclo con valores mezclados.
    CLEAR_BIT(htim3.Instance->CR1, TIM_CR1_CEN);
    __HAL_TIM_SET_AUTORELOAD(&htim3, periodo - 1U);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, pulso);
    __HAL_TIM_SET_COUNTER(&htim3, 0U);
    htim3.Instance->EGR = TIM_EGR_UG;
    __HAL_TIM_CLEAR_FLAG(&htim3, TIM_FLAG_UPDATE);

    if (melodiaEnCurso)
    {
        __HAL_TIM_ENABLE(&htim3);
    }
}

void Audio_Iniciar(void)
{
    Audio_Reproducir(melodiaGalaga, CANTIDAD_NOTAS_GALAGA, 1U);
}

static void Audio_Reproducir(const NotaGalaga *notas, uint32_t cantidad, uint8_t repetir)
{
    if (melodiaEnCurso) Audio_Detener();
    if (notas == NULL || cantidad == 0U) return;
    melodiaActual = notas;
    cantidadNotasActual = cantidad;
    repetirMelodia = repetir;
    notaActual = 0U;
    melodiaEnCurso = 0U;

    // Se conserva esta configuracion aunque CubeMX regenere PSC=0.
    __HAL_TIM_SET_PRESCALER(&htim3, 83U);
    Audio_TocarNota(melodiaActual[notaActual].frecuencia);

    if (HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1) != HAL_OK)
    {
        Error_Handler();
    }

    inicioNota = HAL_GetTick();
    melodiaEnCurso = 1U;
}

void Audio_Actualizar(void)
{
    if (!melodiaEnCurso)
    {
        return;
    }

    uint32_t ahora = HAL_GetTick();

    // La resta sin signo funciona tambien al desbordarse HAL_GetTick.
    if ((uint32_t)(ahora - inicioNota) >= melodiaActual[notaActual].duracion)
    {
        notaActual++;
        if (notaActual >= cantidadNotasActual)
        {
            if (!repetirMelodia)
            {
                Audio_Detener();
                return;
            }
            notaActual = 0U;
        }

        Audio_TocarNota(melodiaActual[notaActual].frecuencia);
        inicioNota = ahora;
    }
}

void Audio_Detener(void)
{
    melodiaEnCurso = 0U;
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 0U);
    htim3.Instance->EGR = TIM_EGR_UG;
    if (HAL_TIM_PWM_Stop(&htim3, TIM_CHANNEL_1) != HAL_OK)
    {
        Error_Handler();
    }
    __HAL_TIM_CLEAR_FLAG(&htim3, TIM_FLAG_UPDATE);
    __HAL_TIM_SET_COUNTER(&htim3, 0U);
    notaActual = 0U;
}


// Cada entrada de estado dibuja su pantalla una sola vez.
static void FSM_CambiarEstado(EstadoJuego nuevo)
{
    if (melodiaEnCurso)
    {
        Audio_Detener();
    }
    estadoJuego = nuevo;
    disparoPendiente = 0;

    switch (estadoJuego)
    {
        case ESTADO_INICIO:
            inicioArmado = 0;
            inicioSolicitado = 0;
            DibujarPantallaInicio();
            Audio_Iniciar();
            break;

        case ESTADO_NIVEL_1:
            contador = 0;
            PrepararNivel();
            break;

        case ESTADO_NIVEL_2:
            PrepararNivel();
            break;

        case ESTADO_GAMEOVER:
        case ESTADO_YOU_WIN:
            Pantalla_Rellenar(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_FONDO);
            if (estadoJuego == ESTADO_GAMEOVER)
            {
                LCD_Print("GAME OVER", (SCREEN_WIDTH - 9 * fontXSizeBig) / 2,
                          100, 2, 0xF800, COLOR_FONDO);
            }
            else
            {
                LCD_Print("YOU WIN", (SCREEN_WIDTH - 7 * fontXSizeBig) / 2,
                          100, 2, 0x07E0, COLOR_FONDO);
            }
            LCD_Print("VOLVIENDO AL INICIO", 40, 145, 1, 0xFFFF, COLOR_FONDO);
            // Inicia el sonido despues de dibujar, para no alargar la primera nota.
            if (estadoJuego == ESTADO_GAMEOVER)
                Audio_Reproducir(melodiaGameOver, CANTIDAD_NOTAS_GAMEOVER, 0U);
            else
                Audio_Reproducir(melodiaVictoria, CANTIDAD_NOTAS_VICTORIA, 0U);
            break;
    }
    // Cuenta la duracion desde que termino de dibujarse la pantalla.
    tiempoEstado = HAL_GetTick();
}

static void PrepararNivel(void)
{
    uint32_t ahora = HAL_GetTick();
    cantidadEnemigos = (estadoJuego == ESTADO_NIVEL_1) ? 1U : MAX_ENEMIGOS;
    naveX = NAVE_X_INICIAL;
    naveY = NAVE_Y_INICIAL;
    balaJugador = (Proyectil){0};
    for (uint8_t i = 0; i < MAX_ENEMIGOS; i++)
    {
        enemigos[i] = (Enemigo){0};
        balasEnemigas[i] = (Proyectil){0};
        if (i < cantidadEnemigos)
        {
            Enemigo *e = &enemigos[i];
            e->vivo = 1;
            e->direccion = (i == 1U) ? -1 : 1;
            e->ultimoDisparo = ahora;
            if (estadoJuego == ESTADO_NIVEL_1)
            {
                e->x = ENEMIGO_X_INICIAL;
                e->y = ENEMIGO_Y_INICIAL;
                e->limiteIzquierdo = LIMITE_IZQUIERDO;
                e->limiteDerecho = LIMITE_DERECHO;
            }
            else
            {
                // Tres zonas separadas evitan que los enemigos se superpongan.
                e->limiteIzquierdo = 8 + 104 * i;
                e->limiteDerecho = e->limiteIzquierdo + 80;
                e->x = e->limiteIzquierdo + 32;
                e->y = 48;
            }
        }
    }
    DibujarFondoEstrellas();
    DibujarContador();
    DibujarActores();
    tiempoFrameAnterior = HAL_GetTick();
    tiempoEstrellasAnterior = tiempoFrameAnterior;
}

// Devuelve un evento solo al confirmar una pulsacion nueva de B1.
static uint8_t ActualizarB1(uint32_t ahora)
{
    uint8_t lectura = (HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin) == B1_PRESIONADO);
    if (lectura != botonB1.crudo)
    {
        botonB1.crudo = lectura;
        botonB1.ultimoCambio = ahora;
    }
    if (botonB1.estable != botonB1.crudo &&
        (uint32_t)(ahora - botonB1.ultimoCambio) >= ANTIRREBOTE_MS)
    {
        botonB1.estable = botonB1.crudo;
        return botonB1.estable;
    }
    return 0;
}

static void FSM_Actualizar(void)
{
    uint32_t ahora = HAL_GetTick();
    uint8_t pulsacion = ActualizarB1(ahora);
    switch (estadoJuego)
    {
        case ESTADO_INICIO:
            Audio_Actualizar();
            // Exige soltar, pulsar y soltar: evita reinicios por B1 sostenido.
            if (!botonB1.estable && !botonB1.crudo &&
                (uint32_t)(ahora - botonB1.ultimoCambio) >= ANTIRREBOTE_MS)
            {
                inicioArmado = 1;
                if (inicioSolicitado)
                {
                    FSM_CambiarEstado(ESTADO_NIVEL_1);
                }
            }
            else if (pulsacion && inicioArmado)
            {
                inicioSolicitado = 1;
                if (melodiaEnCurso) Audio_Detener();
            }
            break;

        case ESTADO_NIVEL_1:
        case ESTADO_NIVEL_2:
            if (pulsacion) disparoPendiente = 1;
            if ((uint32_t)(ahora - tiempoFrameAnterior) >= FRAME_DELAY_MS)
            {
                tiempoFrameAnterior = ahora;
                ActualizarPartida(ahora);
            }
            break;

        case ESTADO_GAMEOVER:
        case ESTADO_YOU_WIN:
            Audio_Actualizar();
            // Conserva el tiempo minimo visible y deja terminar la melodia completa.
            if (!melodiaEnCurso && (uint32_t)(ahora - tiempoEstado) >= RESULTADO_MS)
            {
                FSM_CambiarEstado(ESTADO_INICIO);
            }
            break;
    }
}

static uint8_t EnemigosVivos(void)
{
    uint8_t vivos = 0;
    for (uint8_t i = 0; i < cantidadEnemigos; i++) vivos += enemigos[i].vivo;
    return vivos;
}

static uint8_t Impacta(const Proyectil *p, int x, int y)
{
    // La bala ocupa el centro del sprite; sus margenes negros no hacen dano.
    return p->activa && p->x + 6 * BALA_ESCALA < x + SPRITE_SIZE && p->x + 10 * BALA_ESCALA > x &&
           p->y + 2 * BALA_ESCALA < y + SPRITE_SIZE && p->y + 14 * BALA_ESCALA > y;
}

static void BorrarActores(void)
{
    RestaurarFondoEstrellas(naveX, naveY, SPRITE_SIZE, SPRITE_SIZE);
    if (balaJugador.activa)
        BorrarProyectil(&balaJugador);
    for (uint8_t i = 0; i < cantidadEnemigos; i++)
    {
        if (enemigos[i].vivo)
            RestaurarFondoEstrellas(enemigos[i].x, enemigos[i].y, SPRITE_SIZE, SPRITE_SIZE);
        if (balasEnemigas[i].activa)
            BorrarProyectil(&balasEnemigas[i]);
    }
}

static void BorrarProyectil(const Proyectil *p)
{
    // Recorta al area de juego cuando parte de la bala queda fuera de pantalla.
    int x1 = p->x < 0 ? 0 : p->x;
    int y1 = p->y < MARCADOR_ALTO ? MARCADOR_ALTO : p->y;
    int x2 = p->x + BALA_TAMANO;
    int y2 = p->y + BALA_TAMANO;
    if (x2 > SCREEN_WIDTH) x2 = SCREEN_WIDTH;
    if (y2 > SCREEN_HEIGHT) y2 = SCREEN_HEIGHT;
    if (x2 > x1 && y2 > y1)
        RestaurarFondoEstrellas(x1, y1, x2 - x1, y2 - y1);
}

static void DibujarProyectil(const Proyectil *p, uint8_t haciaAbajo)
{
    if (!p->activa) return;
    // Un bloque de 512 bytes por bala completa, tambien al invertirla.
    Pantalla_Bitmap(p->x, p->y, BALA_TAMANO, BALA_TAMANO,
                    bala, haciaAbajo, MARCADOR_ALTO);
}

static void DibujarActores(void)
{
    Pantalla_Bitmap(naveX, naveY, SPRITE_SIZE, SPRITE_SIZE, nave, 0U, MARCADOR_ALTO);
    for (uint8_t i = 0; i < cantidadEnemigos; i++)
    {
        if (enemigos[i].vivo)
            Pantalla_Bitmap(enemigos[i].x, enemigos[i].y, SPRITE_SIZE, SPRITE_SIZE, enemigoPec, 0U, MARCADOR_ALTO);
        DibujarProyectil(&balasEnemigas[i], 1);
    }
    DibujarProyectil(&balaJugador, 0);
}

static void ActualizarPartida(uint32_t ahora)
{
    // Borra todas las posiciones anteriores antes de mover o dibujar actores.
    BorrarActores();
    int izquierda = HAL_GPIO_ReadPin(BTN_IZQUIERDA_PORT, BTN_IZQUIERDA_PIN) == GPIO_PIN_RESET;
    int derecha = HAL_GPIO_ReadPin(BTN_DERECHA_PORT, BTN_DERECHA_PIN) == GPIO_PIN_RESET;
    if (izquierda && !derecha) naveX -= NAVE_VELOCIDAD;
    if (derecha && !izquierda) naveX += NAVE_VELOCIDAD;
    if (naveX < LIMITE_IZQUIERDO) naveX = LIMITE_IZQUIERDO;
    if (naveX > LIMITE_DERECHO) naveX = LIMITE_DERECHO;

    if (disparoPendiente && !balaJugador.activa)
        balaJugador = (Proyectil){naveX + (SPRITE_SIZE - BALA_TAMANO) / 2, naveY - BALA_TAMANO, 1};
    disparoPendiente = 0;

    for (uint8_t i = 0; i < cantidadEnemigos; i++)
    {
        Enemigo *e = &enemigos[i];
        if (!e->vivo) continue;
        e->x += e->direccion * ENEMIGO_VELOCIDAD;
        if (e->x <= e->limiteIzquierdo)
        {
            e->x = e->limiteIzquierdo;
            e->direccion = 1;
        }
        if (e->x >= e->limiteDerecho)
        {
            e->x = e->limiteDerecho;
            e->direccion = -1;
        }
    }

    if (balaJugador.activa)
    {
        balaJugador.y -= BALA_VELOCIDAD;
        for (uint8_t i = 0; i < cantidadEnemigos; i++)
        {
            if (enemigos[i].vivo && Impacta(&balaJugador, enemigos[i].x, enemigos[i].y))
            {
                enemigos[i].vivo = 0;
                balaJugador.activa = 0;
                contador++;
                DibujarContador();
                break;
            }
        }
        if (balaJugador.y + BALA_TAMANO <= MARCADOR_ALTO) balaJugador.activa = 0;
    }

    // Solo el nivel 2 dispara. Una bala por enemigo limita la dificultad.
    if (estadoJuego == ESTADO_NIVEL_2)
    {
        for (uint8_t i = 0; i < cantidadEnemigos; i++)
        {
            Proyectil *p = &balasEnemigas[i];
            Enemigo *e = &enemigos[i];
            if (p->activa)
            {
                p->y += BALA_ENEMIGA_VELOCIDAD;
                if (Impacta(p, naveX, naveY))
                {
                    // Un impacto termina la partida, incluso si cae el ultimo enemigo.
                    FSM_CambiarEstado(ESTADO_GAMEOVER);
                    return;
                }
                if (p->y >= SCREEN_HEIGHT) p->activa = 0;
            }
            if (e->vivo && !p->activa &&
                (uint32_t)(ahora - e->ultimoDisparo) >=
                DISPARO_ENEMIGO_MS + i * DESFASE_DISPARO_MS)
            {
                *p = (Proyectil){e->x + (SPRITE_SIZE - BALA_TAMANO) / 2, e->y + SPRITE_SIZE, 1};
                e->ultimoDisparo = ahora;
            }
        }
    }

    if (EnemigosVivos() == 0U)
    {
        FSM_CambiarEstado(estadoJuego == ESTADO_NIVEL_1 ? ESTADO_NIVEL_2 : ESTADO_YOU_WIN);
        return;
    }
    ActualizarEstrellas();
    DibujarActores();
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART2_UART_Init();
  MX_SPI1_Init();
  MX_TIM3_Init();
  MX_TIM7_Init();


  /* USER CODE BEGIN 2 */

LCD_Init();
  ConvertirSpriteRGB565(naveBytes, nave, SPRITE_SIZE * SPRITE_SIZE);
  ConvertirSpriteRGB565(enemigoSmllBytes, enemigoPec, SPRITE_SIZE * SPRITE_SIZE);
  ConvertirSpriteRGB565(balaBytes, bala, SPRITE_SIZE * SPRITE_SIZE);

  botonB1.crudo = (HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin) == B1_PRESIONADO);
  botonB1.estable = botonB1.crudo;
  botonB1.ultimoCambio = HAL_GetTick();
  FSM_CambiarEstado(ESTADO_INICIO);

/* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

while (1)
  {
      FSM_Actualizar();

/* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }

  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 83;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 999;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */
  HAL_TIM_MspPostInit(&htim3);

}

/**
  * @brief TIM7 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM7_Init(void)
{

  /* USER CODE BEGIN TIM7_Init 0 */

  /* USER CODE END TIM7_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM7_Init 1 */

  /* USER CODE END TIM7_Init 1 */
  htim7.Instance = TIM7;
  htim7.Init.Prescaler = 0;
  htim7.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim7.Init.Period = 5249;
  htim7.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim7) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim7, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM7_Init 2 */

  /* USER CODE END TIM7_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LCD_RESET_GPIO_Port, LCD_RESET_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, LCD_CS_Pin|SD_CS_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LCD_RESET_Pin */
  GPIO_InitStruct.Pin = LCD_RESET_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;
  HAL_GPIO_Init(LCD_RESET_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : BTN_UP_Pin BTN_DOWN_Pin */
  GPIO_InitStruct.Pin = BTN_UP_Pin|BTN_DOWN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : LCD_DC_Pin */
  GPIO_InitStruct.Pin = LCD_DC_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;
  HAL_GPIO_Init(LCD_DC_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LCD_CS_Pin SD_CS_Pin */
  GPIO_InitStruct.Pin = LCD_CS_Pin|SD_CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */
/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */

  // User can add his own implementation to report the HAL error return state

  __disable_irq();

  while (1)
  {
  }

  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */

  // User can add his own implementation to report the file name and line number,
// ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line)

  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
