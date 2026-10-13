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
#include "galaga_records.h"
#include "galaga_recursos.h"
#include "galaga_juego.h"
#include "galaga_pantalla.h"
#include "ili9341.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* Estados de la interfaz de usuario (maquina de estados principal). */
typedef enum
{
    UI_PORTADA,
    UI_MENU,
    UI_JUEGO,
    UI_RESULTADO
} EstadoUI;

/* Estado de un boton fisico con antirrebote por tiempo. */
typedef struct
{
    uint8_t  crudo;     /* Lectura instantanea del pin (1 = presionado)        */
    uint8_t  estable;   /* Lectura ya filtrada por el antirrebote              */
    uint32_t cambio;    /* Instante (ms) del ultimo cambio de la lectura cruda */
} BotonFiltrado;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* --- Tiempos de la interfaz --- */
#define ANTIRREBOTE_MS      30U     /* Tiempo minimo estable para aceptar un boton */
#define RESULTADO_MS        2500U   /* Duracion de la pantalla de resultado        */

/* --- Protocolo del control por UART: [INICIO][BOTONES][~BOTONES][FIN] --- */
#define CONTROL_INICIO      0xA5U
#define CONTROL_FIN         0x5AU
#define CONTROL_TIMEOUT_MS  250U    /* Sin paquetes validos en este tiempo: se sueltan los botones */

/* --- Mascara de bits del byte BOTONES --- */
#define CONTROL_IZQ         (1U << 0)
#define CONTROL_DER         (1U << 1)
#define CONTROL_ACCION      (1U << 2)
#define CONTROL_ATRAS       (1U << 3)
#define CONTROL_ARRIBA      (1U << 4)
#define CONTROL_ABAJO       (1U << 5)

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef hspi1;
SPI_HandleTypeDef hspi2;

TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim7;

UART_HandleTypeDef huart2;
UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */
// Tiempo real de partida, congelado al comenzar la celebracion.
static uint32_t inicioPartida, duracionVictoria;
static uint8_t tiempoCongelado;


/* --- Juego e interfaz --- */
static GJuego    juego;
static EstadoUI  estadoUI;
static uint8_t   nivelSeleccionado = 1;
static uint32_t  tiempoUI;          /* Instante en que se entro al estado actual */
static uint32_t  tiempoPaso;        /* Instante del siguiente paso de simulacion */
volatile uint32_t galagaPasosDescartados;

/* --- Botones fisicos --- */
static BotonFiltrado botonB1;
static BotonFiltrado botonIzq;
static BotonFiltrado botonDer;
static uint8_t b1Armado;            /* B1 ya fue soltado, se permite una nueva pulsacion */
static uint8_t b1Solicitado;        /* B1 fue presionado y espera a ser soltado          */
static uint8_t disparoPendiente;    /* Disparo solicitado, se consume en el siguiente paso */

/* --- Audio --- */
static uint32_t          notaActual = 0;
static uint32_t          inicioNota = 0;
static uint8_t           melodiaEnCurso = 0;
static const NotaGalaga *melodiaActual = melodiaGalaga;
static uint32_t          cantidadNotasActual = CANTIDAD_NOTAS_GALAGA;
static uint8_t           repetirMelodia = 1;

/* --- Control por UART (se escriben desde la interrupcion) --- */
static volatile uint8_t  uartRxByte;
static volatile uint8_t  controlRxEstado;
static volatile uint8_t  controlRxBotones;
static volatile uint8_t  controlBotones;
static volatile uint32_t controlUltimoPaquete;
static uint8_t           controlAnterior;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_SPI1_Init(void);
static void MX_SPI2_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_TIM3_Init(void);
static void MX_TIM7_Init(void);
/* USER CODE BEGIN PFP */

/* Audio */
void Audio_Iniciar(void);
void Audio_Detener(void);
void Audio_Actualizar(void);
static void Audio_TocarNota(uint16_t frecuencia);
static void Audio_Reproducir(const NotaGalaga *notas, uint32_t cantidad, uint8_t repetir);

/* Control por UART */
static void Control_Iniciar(void);
static void Control_ProcesarByte(uint8_t dato);

/* Botones e interfaz */
static uint8_t Boton_Actualizar(BotonFiltrado *b, GPIO_TypeDef *puerto, uint16_t pin, uint32_t ahora);
static void UI_Entrar(EstadoUI nuevo);
static void UI_Actualizar(void);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* ==========================================================================
 *  AUDIO
 * ========================================================================== */

/**
  * @brief  Reproduce una nota por PWM en TIM3 (canal 1). Frecuencia 0 = silencio.
  * @param  frecuencia: Frecuencia de la nota en Hz.
  */
static void Audio_TocarNota(uint16_t frecuencia)
{
    // TIM3 recibe 84 MHz: PSC=83 produce un contador de 1 MHz.
    // Durante las pausas se mantiene el temporizador con CCR1=0.
    uint32_t periodo = 1000U;
    uint32_t pulso = 0U;

    if (frecuencia != 0U)
    {
        periodo = (1000000U + frecuencia / 2U) / frecuencia;
        pulso = periodo / 2U;   /* Ciclo de trabajo del 50 % */
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

/**
  * @brief  Inicia la melodia principal de Galaga en modo repetitivo.
  */
void Audio_Iniciar(void)
{
    Audio_Reproducir(melodiaGalaga, CANTIDAD_NOTAS_GALAGA, 1U);
}

/**
  * @brief  Comienza a reproducir una melodia.
  * @param  notas: Arreglo de notas (frecuencia y duracion).
  * @param  cantidad: Numero de notas del arreglo.
  * @param  repetir: 1 = repetir en bucle, 0 = reproducir una sola vez.
  */
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

/**
  * @brief  Avanza la melodia segun el tiempo (no bloqueante). Se llama en cada
  *         vuelta del bucle principal.
  */
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

/**
  * @brief  Detiene la melodia y apaga la salida PWM.
  */
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

/* ==========================================================================
 *  CONTROL POR UART (paquete: INICIO, BOTONES, ~BOTONES, FIN)
 * ========================================================================== */

/**
  * @brief  Reinicia el estado del control y habilita la recepcion por interrupcion.
  */
static void Control_Iniciar(void)
{
    controlRxEstado = 0U;
    controlRxBotones = 0U;
    controlBotones = 0U;
    controlAnterior = 0U;
    controlUltimoPaquete = HAL_GetTick();

    HAL_NVIC_SetPriority(USART1_IRQn, 1U, 0U);
    HAL_NVIC_EnableIRQ(USART1_IRQn);

    if (HAL_UART_Receive_IT(&huart1,
                            (uint8_t *)&uartRxByte, 1U) != HAL_OK)
    {
        Error_Handler();
    }
}

/**
  * @brief  Maquina de estados que valida cada byte recibido del control.
  *         0: espera INICIO | 1: recibe BOTONES | 2: valida ~BOTONES | 3: espera FIN
  * @param  dato: Byte recibido por la UART.
  */
static void Control_ProcesarByte(uint8_t dato)
{
    switch (controlRxEstado)
    {
        case 0:
            if (dato == CONTROL_INICIO)
            {
                controlRxEstado = 1U;
            }
            break;

        case 1:
            controlRxBotones = dato;
            controlRxEstado = 2U;
            break;

        case 2:
            if (dato == (uint8_t)(~controlRxBotones))
            {
                controlRxEstado = 3U;
            }
            else
            {
                controlRxEstado = (dato == CONTROL_INICIO) ? 1U : 0U;
            }
            break;

        case 3:
            if (dato == CONTROL_FIN)
            {
                controlBotones = controlRxBotones;
                controlUltimoPaquete = HAL_GetTick();
            }

            controlRxEstado = 0U;
            break;

        default:
            controlRxEstado = 0U;
            break;
    }
}


/* ==========================================================================
 *  BOTONES
 * ========================================================================== */

/**
  * @brief  Filtra pulsaciones de los tres botones sin detener el bucle.
  * @param  b: Estado del boton a actualizar.
  * @param  puerto: Puerto GPIO del boton.
  * @param  pin: Pin del boton.
  * @param  ahora: Tiempo actual en ms.
  * @retval Valor estable del boton cuando este cambia; 0 si no hubo cambio.
  */
static uint8_t Boton_Actualizar(BotonFiltrado *b, GPIO_TypeDef *puerto, uint16_t pin, uint32_t ahora)
{
    uint8_t lectura = HAL_GPIO_ReadPin(puerto, pin) == GPIO_PIN_RESET;

    if (lectura != b->crudo)
    {
        b->crudo = lectura;
        b->cambio = ahora;
    }

    if (b->estable != b->crudo && (uint32_t)(ahora - b->cambio) >= ANTIRREBOTE_MS)
    {
        b->estable = b->crudo;
        return b->estable;
    }

    return 0;
}

/* ==========================================================================
 *  INTERFAZ DE USUARIO
 * ========================================================================== */

/**
  * @brief  Cambia de estado de la interfaz y ejecuta las acciones de entrada.
  * @param  nuevo: Estado al que se desea entrar.
  */
static void UI_Entrar(EstadoUI nuevo)
{
    estadoUI = nuevo;
    b1Armado = 0;
    b1Solicitado = 0;
    disparoPendiente = 0;

    if (nuevo == UI_PORTADA)
    {
        GP_Portada();
        Audio_Iniciar();
    }
    else if (nuevo == UI_MENU)
    {
        GR_Comprobar();
        GP_Menu(nivelSeleccionado);
        if (!melodiaEnCurso) Audio_Iniciar();
    }
    else if (nuevo == UI_JUEGO)
    {
        if (melodiaEnCurso) Audio_Detener();

        G_Iniciar(&juego, nivelSeleccionado);
        GP_PrepararJuego();
        GP_Dibujar(&juego);
        GP_Esperar();

        tiempoPaso = HAL_GetTick();
        inicioPartida = tiempoPaso;
        duracionVictoria = 0;
        tiempoCongelado = 0;
    }
    else    /* UI_RESULTADO */
    {
        if (melodiaEnCurso) Audio_Detener();

        // Esperar el ultimo dibujo y guardar antes de iniciar la melodia.
        GP_Esperar();
        uint8_t nuevoRecord = 0;
        if (juego.resultado == G_VICTORIA && tiempoCongelado)
            nuevoRecord = GR_GuardarVictoria(juego.nivel, duracionVictoria);
        GP_Resultado(juego.resultado == G_VICTORIA);
        GP_RecordResultado(juego.resultado == G_VICTORIA, duracionVictoria, nuevoRecord);

        if (juego.resultado == G_VICTORIA)
            Audio_Reproducir(melodiaVictoria, CANTIDAD_NOTAS_VICTORIA, 0);
        else
            Audio_Reproducir(melodiaGameOver, CANTIDAD_NOTAS_GAMEOVER, 0);
    }

    tiempoUI = HAL_GetTick();
}

/**
  * @brief  Actualiza la interfaz segun el estado actual. Se llama en cada vuelta
  *         del bucle principal (no bloqueante).
  */
static void UI_Actualizar(void)
{
    GP_Servicio();

    uint32_t ahora = HAL_GetTick();

    /* Si el control deja de enviar paquetes, se sueltan todos sus botones. */
    if ((uint32_t)(ahora - controlUltimoPaquete) > CONTROL_TIMEOUT_MS)
    {
        controlBotones = 0U;
    }

    /* Estado actual del control y deteccion de flancos de pulsacion nueva. */
    uint8_t controlActual = controlBotones;
    uint8_t controlIzq = (controlActual & CONTROL_IZQ) != 0U;
    uint8_t controlDer = (controlActual & CONTROL_DER) != 0U;
    uint8_t controlAccion = (controlActual & CONTROL_ACCION) != 0U;

    uint8_t controlIzqNuevo = controlIzq && !(controlAnterior & CONTROL_IZQ);
    uint8_t controlDerNuevo = controlDer && !(controlAnterior & CONTROL_DER);
    uint8_t controlAccionNuevo = controlAccion && !(controlAnterior & CONTROL_ACCION);

    controlAnterior = controlActual;

    /* Botones fisicos con antirrebote. */
    uint8_t b1 = Boton_Actualizar(&botonB1, B1_GPIO_Port, B1_Pin, ahora);
    uint8_t izq = Boton_Actualizar(&botonIzq, BTN_UP_GPIO_Port, BTN_UP_Pin, ahora);
    uint8_t der = Boton_Actualizar(&botonDer, BTN_DOWN_GPIO_Port, BTN_DOWN_Pin, ahora);

    Audio_Actualizar();

    /* ---------------- Portada y menu ---------------- */
    if (estadoUI == UI_PORTADA || estadoUI == UI_MENU)
    {
        uint8_t menuIzq = izq || controlIzqNuevo;
        uint8_t menuDer = der || controlDerNuevo;

        /* Cambio de nivel (solo en el menu). */
        if (estadoUI == UI_MENU && menuIzq != menuDer)
        {
            if (menuIzq)
                nivelSeleccionado = (nivelSeleccionado == 1) ? 3 : nivelSeleccionado - 1;
            else
                nivelSeleccionado = (nivelSeleccionado == 3) ? 1 : nivelSeleccionado + 1;

            GP_Menu(nivelSeleccionado);
        }

        // El control de PlayStation confirma con una nueva pulsacion de X.
        if (controlAccionNuevo)
        {
            if (estadoUI == UI_PORTADA)
                UI_Entrar(UI_MENU);
            else
                UI_Entrar(UI_JUEGO);

            return;
        }

        // Confirmar al soltar; la misma pulsacion fisica nunca atraviesa dos menus.
        if (!botonB1.estable && !botonB1.crudo &&
            (uint32_t)(ahora - botonB1.cambio) >= ANTIRREBOTE_MS)
        {
            b1Armado = 1;
            if (b1Solicitado)
            {
                UI_Entrar(estadoUI == UI_PORTADA ? UI_MENU : UI_JUEGO);
                return;
            }
        }
        else if (b1 && b1Armado)
        {
            b1Solicitado = 1;
        }
    }
    /* ---------------- Juego ---------------- */
    else if (estadoUI == UI_JUEGO)
    {
        if (b1 || controlAccionNuevo)
            disparoPendiente = 1U;

        uint32_t pendientes = (uint32_t)(ahora - tiempoPaso) / G_PASO_MS;
        if (!pendientes)
            return;

        // Recuperar hasta tres pasos; descartar atrasos extremos evita una
        // espiral de actualizaciones si el bus o el debugger se detienen.
        if (pendientes > 3U)
        {
            galagaPasosDescartados += pendientes - 3U;
            tiempoPaso += (pendientes - 3U) * G_PASO_MS;
            pendientes = 3U;
        }

        GP_Marcar(&juego);

        for (uint32_t i = 0; i < pendientes; ++i)
        {
            GEntrada entrada =
            {
                botonIzq.estable || controlIzq,
                botonDer.estable || controlDer,
                disparoPendiente
            };

            disparoPendiente = 0U;
            G_Paso(&juego, entrada);
            // Eliminar la ultima amenaza congela el reloj antes del confeti final.
            if (!tiempoCongelado && juego.celebracion)
            {
                duracionVictoria = HAL_GetTick() - inicioPartida;
                tiempoCongelado = 1;
            }
            tiempoPaso += G_PASO_MS;

            if (juego.resultado != G_JUGANDO)
            {
                UI_Entrar(UI_RESULTADO);
                return;
            }
        }

        GP_Dibujar(&juego);
    }
    /* ---------------- Resultado ---------------- */
    else if (!melodiaEnCurso && (uint32_t)(ahora - tiempoUI) >= RESULTADO_MS)
    {
        UI_Entrar(UI_MENU);
    }
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
  MX_SPI2_Init();
  MX_USART1_UART_Init();
  MX_TIM3_Init();
  MX_TIM7_Init();
  /* USER CODE BEGIN 2 */

  /* Pantalla */
  LCD_Init();
  GP_Iniciar();


  /* Estado inicial de los botones: se lee el pin para evitar una pulsacion falsa al arrancar. */
  botonB1.crudo = botonB1.estable = (HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin) == GPIO_PIN_RESET);
  botonIzq.crudo = botonIzq.estable = (HAL_GPIO_ReadPin(BTN_UP_GPIO_Port, BTN_UP_Pin) == GPIO_PIN_RESET);
  botonDer.crudo = botonDer.estable = (HAL_GPIO_ReadPin(BTN_DOWN_GPIO_Port, BTN_DOWN_Pin) == GPIO_PIN_RESET);
  botonB1.cambio = botonIzq.cambio = botonDer.cambio = HAL_GetTick();

  /* Control por UART y pantalla de portada */
  GR_Iniciar();
  Control_Iniciar();
  UI_Entrar(UI_PORTADA);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    UI_Actualizar();
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
// Configuracion reflejada en ILI9341SPI.ioc.
static void MX_SPI2_Init(void)
{
  /* USER CODE BEGIN SPI2_Init 0 */
  /* USER CODE END SPI2_Init 0 */
  /* USER CODE BEGIN SPI2_Init 1 */
  /* USER CODE END SPI2_Init 1 */
  hspi2.Instance = SPI2;
  hspi2.Init.Mode = SPI_MODE_MASTER;
  hspi2.Init.Direction = SPI_DIRECTION_2LINES;
  hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi2.Init.NSS = SPI_NSS_SOFT;
  hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_256;
  hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi2.Init.CRCPolynomial = 10;
  // FatFs comprueba la inicializacion; un fallo SD no detiene el juego.
  HAL_SPI_Init(&hspi2);
  /* USER CODE BEGIN SPI2_Init 2 */
  /* USER CODE END SPI2_Init 2 */
}

static void MX_USART1_UART_Init(void)
{
  /* USER CODE BEGIN USART1_Init 0 */
  /* USER CODE END USART1_Init 0 */
  /* USER CODE BEGIN USART1_Init 1 */
  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK) Error_Handler();
  /* USER CODE BEGIN USART1_Init 2 */
  /* USER CODE END USART1_Init 2 */
}

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
  HAL_GPIO_WritePin(GPIOB, LCD_CS_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_SET);

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

/**
  * @brief  Callback de recepcion completa de la UART: procesa el byte recibido
  *         y vuelve a habilitar la recepcion del siguiente.
  * @param  huart: Manejador de la UART que genero la interrupcion.
  */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        Control_ProcesarByte(uartRxByte);

        HAL_UART_Receive_IT(&huart1,
                            (uint8_t *)&uartRxByte, 1U);
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        controlRxEstado = 0U;
        controlBotones = 0U;

        HAL_UART_Receive_IT(&huart1,
                            (uint8_t *)&uartRxByte, 1U);
    }
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
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
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
