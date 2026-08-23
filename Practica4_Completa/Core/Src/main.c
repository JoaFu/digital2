/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
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

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
typedef enum {
    ESTADO_ESPERA = 0,
    ESTADO_CUENTA_REGRESIVA,
    ESTADO_CARRERA,
    ESTADO_FIN
} EstadoJuego;

volatile EstadoJuego estado = ESTADO_ESPERA;

volatile uint8_t contadorJ1 = 0;
volatile uint8_t contadorJ2 = 0;

// Antirrebote
#define DEBOUNCE_MS 100U
volatile uint32_t tiempoInicio = 0;
volatile uint32_t tiempoJ1 = 0;
volatile uint32_t tiempoJ2 = 0;

// Cuenta regresiva
#define INTERVALO_CUENTA_MS 1000U
volatile int8_t  numeroCuenta = 5;
volatile uint32_t tiempoCuentaRegresiva = 0;

// LEDs jugador 1 (PA0, PA1, PA6, PA7)
GPIO_TypeDef* ledJ1_port[4] = {GPIOA, GPIOA, GPIOA, GPIOA};
const uint16_t ledJ1_pin[4] = {GPIO_PIN_0, GPIO_PIN_1, GPIO_PIN_6, GPIO_PIN_7};

// LEDs jugador 2 (PB0, PB1, PB4, PB5)
GPIO_TypeDef* ledJ2_port[4] = {GPIOB, GPIOB, GPIOB, GPIOB};
const uint16_t ledJ2_pin[4] = {GPIO_PIN_0, GPIO_PIN_1, GPIO_PIN_4, GPIO_PIN_5};

// Display 7 segmentos a,b,c,d,e,f,g
GPIO_TypeDef* segPort[7] = {GPIOB, GPIOB, GPIOB, GPIOB, GPIOB, GPIOB, GPIOB};
const uint16_t segPin[7] = {GPIO_PIN_6, GPIO_PIN_7, GPIO_PIN_8, GPIO_PIN_9,
                             GPIO_PIN_10, GPIO_PIN_12, GPIO_PIN_13};
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
/* USER CODE BEGIN PFP */
void ActualizarLedsJ1(uint8_t contador);
void ActualizarLedsJ2(uint8_t contador);
void MostrarDigito(uint8_t n);
void ProcesarCuentaRegresiva(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

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
  /* USER CODE BEGIN 2 */

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	  ProcesarCuentaRegresiva();
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
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 64;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
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
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV4;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
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
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0|GPIO_PIN_1|LD2_Pin|GPIO_PIN_6
                          |GPIO_PIN_7, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_10|GPIO_PIN_12
                          |GPIO_PIN_13|GPIO_PIN_4|GPIO_PIN_5|GPIO_PIN_6
                          |GPIO_PIN_7|GPIO_PIN_8|GPIO_PIN_9, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : PA0 PA1 LD2_Pin PA6
                           PA7 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|LD2_Pin|GPIO_PIN_6
                          |GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : USART_TX_Pin USART_RX_Pin */
  GPIO_InitStruct.Pin = USART_TX_Pin|USART_RX_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PB0 PB1 PB10 PB12
                           PB13 PB4 PB5 PB6
                           PB7 PB8 PB9 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_10|GPIO_PIN_12
                          |GPIO_PIN_13|GPIO_PIN_4|GPIO_PIN_5|GPIO_PIN_6
                          |GPIO_PIN_7|GPIO_PIN_8|GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : PC10 PC11 PC12 */
  GPIO_InitStruct.Pin = GPIO_PIN_10|GPIO_PIN_11|GPIO_PIN_12;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);


  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void ActualizarLedsJ1(uint8_t contador)
{
    for (uint8_t i = 0; i < 4; i++)
    {
        if (i < contador)
        {
            HAL_GPIO_WritePin(ledJ1_port[i], ledJ1_pin[i], GPIO_PIN_SET);
        }
        else
        {
            HAL_GPIO_WritePin(ledJ1_port[i], ledJ1_pin[i], GPIO_PIN_RESET);
        }
    }
}

void ActualizarLedsJ2(uint8_t contador)
{
    for (uint8_t i = 0; i < 4; i++)
    {
    	if (i < contador)
    	{
    		HAL_GPIO_WritePin(ledJ2_port[i], ledJ2_pin[i], GPIO_PIN_SET);
    	}
    	else
    	{
    		HAL_GPIO_WritePin(ledJ2_port[i], ledJ2_pin[i], GPIO_PIN_RESET);
    	}
    }
}

/* Patrones a,b,c,d,e,f,g -- asume cátodo común (1 = segmento ON).
   Si tu display es ánodo común, invierte la lógica: usa GPIO_PIN_RESET
   cuando el patrón valga 1, y GPIO_PIN_SET cuando valga 0. */
void MostrarDigito(uint8_t n)
{
    static const uint8_t patrones[6][7] = {
      /* a  b  c  d  e  f  g */
        {1, 1, 1, 1, 1, 1, 0}, // 0
        {0, 1, 1, 0, 0, 0, 0}, // 1
        {1, 1, 0, 1, 1, 0, 1}, // 2
        {1, 1, 1, 1, 0, 0, 1}, // 3
        {0, 1, 1, 0, 0, 1, 1}, // 4
        {1, 0, 1, 1, 0, 1, 1}, // 5
    };

    if (n > 5) return;

    for (uint8_t i = 0; i < 7; i++)
    {
        if (patrones[n][i])
        {
            HAL_GPIO_WritePin(segPort[i], segPin[i], GPIO_PIN_SET);
        }
        else
        {
            HAL_GPIO_WritePin(segPort[i], segPin[i], GPIO_PIN_RESET);
        }
    }
}

/* Se llama en cada vuelta del while(1) principal. No bloquea nada. */
void ProcesarCuentaRegresiva(void)
{
    if (estado != ESTADO_CUENTA_REGRESIVA) return;

    if ((HAL_GetTick() - tiempoCuentaRegresiva) >= INTERVALO_CUENTA_MS)
    {
        tiempoCuentaRegresiva = HAL_GetTick();

        if (numeroCuenta == 0)
        {
            estado = ESTADO_CARRERA;   // fin del conteo, arranca la carrera
        }
        else
        {
            numeroCuenta--;
            MostrarDigito(numeroCuenta);
        }
    }
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    uint32_t ahora = HAL_GetTick();

    /* ---------------- INICIO / REINICIO - PC10 ---------------- */
    if (GPIO_Pin == GPIO_PIN_10)
    {
        if ((ahora - tiempoInicio) >= DEBOUNCE_MS &&
            HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_10) == GPIO_PIN_RESET)
        {
            tiempoInicio = ahora;

            if (estado == ESTADO_ESPERA || estado == ESTADO_FIN)
            {
                contadorJ1 = 0;
                contadorJ2 = 0;
                ActualizarLedsJ1(0);
                ActualizarLedsJ2(0);

                numeroCuenta = 5;
                MostrarDigito(numeroCuenta);
                tiempoCuentaRegresiva = ahora;

                estado = ESTADO_CUENTA_REGRESIVA;
            }
        }
    }

    /* ---------------- JUGADOR 1 - PC11 ---------------- */
    else if (GPIO_Pin == GPIO_PIN_11)
    {
        if ((ahora - tiempoJ1) >= DEBOUNCE_MS &&
            HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_11) == GPIO_PIN_RESET)
        {
            tiempoJ1 = ahora;

            if (estado == ESTADO_CARRERA && contadorJ1 < 4)
            {
                contadorJ1++;
                ActualizarLedsJ1(contadorJ1);

                if (contadorJ1 == 4)
                {
                    ActualizarLedsJ2(0);   // apaga al contrario
                    MostrarDigito(1);      // "1" = ganó J1
                    estado = ESTADO_FIN;
                }
            }
        }
    }

    /* ---------------- JUGADOR 2 - PC12 ---------------- */
    else if (GPIO_Pin == GPIO_PIN_12)
    {
        if ((ahora - tiempoJ2) >= DEBOUNCE_MS &&
            HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_12) == GPIO_PIN_RESET)
        {
            tiempoJ2 = ahora;

            if (estado == ESTADO_CARRERA && contadorJ2 < 4)
            {
                contadorJ2++;
                ActualizarLedsJ2(contadorJ2);

                if (contadorJ2 == 4)
                {
                    ActualizarLedsJ1(0);   // apaga al contrario
                    MostrarDigito(2);      // "2" = ganó J2
                    estado = ESTADO_FIN;
                }
            }
        }
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
