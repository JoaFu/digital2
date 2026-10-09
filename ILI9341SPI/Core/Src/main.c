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
#include "galaga_recursos.h"
#include "galaga_juego.h"
#include "galaga_pantalla.h"
#include "ili9341.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum { UI_PORTADA, UI_MENU, UI_JUEGO, UI_RESULTADO } EstadoUI;
typedef struct { uint8_t crudo, estable; uint32_t cambio; } BotonFiltrado;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define ANTIRREBOTE_MS 30U
#define RESULTADO_MS 2500U
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
static GJuego juego;
static EstadoUI estadoUI;
static BotonFiltrado botonB1, botonIzq, botonDer;
static uint8_t nivelSeleccionado=1, b1Armado, b1Solicitado, disparoPendiente;
static uint32_t tiempoUI, tiempoPaso;
volatile uint32_t galagaPasosDescartados;
static uint32_t notaActual=0, inicioNota=0;
static uint8_t melodiaEnCurso=0;
static const NotaGalaga *melodiaActual=melodiaGalaga;
static uint32_t cantidadNotasActual=CANTIDAD_NOTAS_GALAGA;
static uint8_t repetirMelodia=1;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_SPI1_Init(void);
static void MX_TIM3_Init(void);
static void MX_TIM7_Init(void);
/* USER CODE BEGIN PFP */
void Audio_Iniciar(void);
void Audio_Detener(void);
void Audio_Actualizar(void);
static void Audio_TocarNota(uint16_t frecuencia);
static void Audio_Reproducir(const NotaGalaga *notas,uint32_t cantidad,uint8_t repetir);
static void UI_Actualizar(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
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



/* Filtra pulsaciones de los tres botones sin detener el bucle. */
static uint8_t Boton_Actualizar(BotonFiltrado *b,GPIO_TypeDef *puerto,uint16_t pin,uint32_t ahora)
{
    uint8_t lectura=HAL_GPIO_ReadPin(puerto,pin)==GPIO_PIN_RESET;
    if(lectura!=b->crudo) { b->crudo=lectura; b->cambio=ahora; }
    if(b->estable!=b->crudo && (uint32_t)(ahora-b->cambio)>=ANTIRREBOTE_MS) {
        b->estable=b->crudo; return b->estable;
    }
    return 0;
}
static void UI_Entrar(EstadoUI nuevo)
{
    estadoUI=nuevo; b1Armado=0; b1Solicitado=0; disparoPendiente=0;
    if(nuevo==UI_PORTADA) { GP_Portada(); Audio_Iniciar(); }
    else if(nuevo==UI_MENU) { GP_Menu(nivelSeleccionado); if(!melodiaEnCurso) Audio_Iniciar(); }
    else if(nuevo==UI_JUEGO) {
        if(melodiaEnCurso) Audio_Detener();
        G_Iniciar(&juego,nivelSeleccionado); GP_PrepararJuego(); GP_Dibujar(&juego);
        tiempoPaso=HAL_GetTick();
    } else {
        if(melodiaEnCurso) Audio_Detener();
        GP_Resultado(juego.resultado==G_VICTORIA);
        if(juego.resultado==G_VICTORIA) Audio_Reproducir(melodiaVictoria,CANTIDAD_NOTAS_VICTORIA,0);
        else Audio_Reproducir(melodiaGameOver,CANTIDAD_NOTAS_GAMEOVER,0);
    }
    tiempoUI=HAL_GetTick();
}
static void UI_Actualizar(void)
{
    uint32_t ahora=HAL_GetTick();
    uint8_t b1=Boton_Actualizar(&botonB1,B1_GPIO_Port,B1_Pin,ahora);
    uint8_t izq=Boton_Actualizar(&botonIzq,BTN_UP_GPIO_Port,BTN_UP_Pin,ahora);
    uint8_t der=Boton_Actualizar(&botonDer,BTN_DOWN_GPIO_Port,BTN_DOWN_Pin,ahora);
    Audio_Actualizar();
    if(estadoUI==UI_PORTADA || estadoUI==UI_MENU) {
        if(estadoUI==UI_MENU && izq!=der) {
            if(izq) nivelSeleccionado=nivelSeleccionado==1?3:nivelSeleccionado-1;
            else nivelSeleccionado=nivelSeleccionado==3?1:nivelSeleccionado+1;
            GP_Menu(nivelSeleccionado);
        }
        /* Confirmar al soltar; la misma pulsacion nunca atraviesa dos menus. */
        if(!botonB1.estable && !botonB1.crudo && (uint32_t)(ahora-botonB1.cambio)>=ANTIRREBOTE_MS) {
            b1Armado=1;
            if(b1Solicitado) UI_Entrar(estadoUI==UI_PORTADA?UI_MENU:UI_JUEGO);
        } else if(b1 && b1Armado) b1Solicitado=1;
    } else if(estadoUI==UI_JUEGO) {
        if(b1) disparoPendiente=1;
        uint32_t pendientes=(uint32_t)(ahora-tiempoPaso)/G_PASO_MS;
        if(!pendientes) return;
        /* Recuperar hasta tres pasos; descartar atrasos extremos evita una
           espiral de actualizaciones si el bus o el debugger se detienen. */
        if(pendientes>3U) {
            galagaPasosDescartados+=pendientes-3U;
            tiempoPaso+=(pendientes-3U)*G_PASO_MS; pendientes=3U;
        }
        GP_Marcar(&juego);
        for(uint32_t i=0;i<pendientes;++i) {
            GEntrada entrada={botonIzq.estable,botonDer.estable,disparoPendiente};
            disparoPendiente=0; G_Paso(&juego,entrada); tiempoPaso+=G_PASO_MS;
            if(juego.resultado!=G_JUGANDO) { UI_Entrar(UI_RESULTADO); return; }
        }
        GP_Dibujar(&juego);
    } else if(!melodiaEnCurso && (uint32_t)(ahora-tiempoUI)>=RESULTADO_MS) UI_Entrar(UI_MENU);
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
  GP_Iniciar();
  botonB1.crudo=botonB1.estable=(HAL_GPIO_ReadPin(B1_GPIO_Port,B1_Pin)==GPIO_PIN_RESET);
  botonIzq.crudo=botonIzq.estable=(HAL_GPIO_ReadPin(BTN_UP_GPIO_Port,BTN_UP_Pin)==GPIO_PIN_RESET);
  botonDer.crudo=botonDer.estable=(HAL_GPIO_ReadPin(BTN_DOWN_GPIO_Port,BTN_DOWN_Pin)==GPIO_PIN_RESET);
  botonB1.cambio=botonIzq.cambio=botonDer.cambio=HAL_GetTick();
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
