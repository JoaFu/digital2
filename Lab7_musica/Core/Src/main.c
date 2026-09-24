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
#include <math.h>
#include <string.h>

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
DAC_HandleTypeDef hdac;
DMA_HandleTypeDef hdma_dac1;

TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim6;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
#define SAMPLE_RATE       8000U
#define DAC_MIDDLE        128U
#define DAC_AMPLITUDE     95.0f
#define AUDIO_BUFFER_SIZE 14000U
#define PI_F              3.14159265358979323846f
#define PWM_COUNTER_HZ 1000000U

static uint8_t dacBuffer[AUDIO_BUFFER_SIZE];

static volatile uint8_t dacFinished = 0;
static volatile uint8_t playRequest = 0;
static volatile uint8_t playing = 0;
static uint8_t uartRx;

/*
FelizNav:d=8,o=5,b=140:
a,4d6,c#6,d6,2b.,4p,b,4e6,d6,b,2a.,4p,
a,4d6,c#6,d6,4b.,g,4b,4b,a,a,b,a,4g,g,1f#
*/

static const uint16_t noteFrequency[] = {
    880, 1175, 1109, 1175, 988, 0,
    988, 1319, 1175, 988, 880, 0,
    880, 1175, 1109, 1175, 988, 784,
    988, 988, 880, 880, 988, 880,
    784, 784, 740
};

static const uint16_t noteDurationMs[] = {
    214, 429, 214, 214, 1286, 429,
    214, 429, 214, 214, 1286, 429,
    214, 429, 214, 214, 643, 214,
    429, 429, 214, 214, 214, 214,
    429, 214, 1714
};

#define NOTE_COUNT \
    (sizeof(noteFrequency) / sizeof(noteFrequency[0]))

/*
SuperMar:d=4,o=5,b=125:
a,8f.,16c,16d,16f,16p,f,16d,16c,16p,16f,16p,16f,16p,
8c6,8a.,g,16c,a,8f.,16c,16d,16f,16p,f,16d,16c,16p,
16f,16p,16a#,16a,16g,2f,16p,8a.,8f.,8c,8a.,f,16g#,
16f,16c,16p,8g#.,2g,8a.,8f.,8c,8a.,f,16g#,16f,8c,2c6
*/

static const uint16_t marioFrequency[] = {
    880, 698, 523, 587, 698, 0, 698, 587, 523, 0,
    698, 0, 698, 0, 1047, 880, 784, 523, 880, 698,
    523, 587, 698, 0, 698, 587, 523, 0, 698, 0,
    932, 880, 784, 698, 0, 880, 698, 523, 880, 698,
    831, 698, 523, 0, 831, 784, 880, 698, 523, 880,
    698, 831, 698, 523, 1047
};

static const uint16_t marioDurationMs[] = {
    480, 360, 120, 120, 120, 120, 480, 120, 120, 120,
    120, 120, 120, 120, 240, 360, 480, 120, 480, 360,
    120, 120, 120, 120, 480, 120, 120, 120, 120, 120,
    120, 120, 120, 960, 120, 360, 360, 240, 360, 480,
    120, 120, 120, 120, 360, 960, 360, 360, 240, 360,
    480, 120, 120, 240, 960
};

#define MARIO_NOTE_COUNT \
    (sizeof(marioFrequency) / sizeof(marioFrequency[0]))

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_DAC_Init(void);
static void MX_TIM6_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_TIM2_Init(void);
/* USER CODE BEGIN PFP */
static uint32_t GenerateNote(uint16_t frequency, uint16_t durationMs);
static void PlayFelizNavidadDAC(void);
static void PlaySuperMarioPWM(void);
static void ShowMenu(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static uint32_t GenerateNote(uint16_t frequency, uint16_t durationMs)
{
    uint32_t sampleCount =
        ((uint32_t)durationMs * SAMPLE_RATE) / 1000U;

    if (sampleCount > AUDIO_BUFFER_SIZE)
    {
        sampleCount = AUDIO_BUFFER_SIZE;
    }

    if (frequency == 0U)
    {
        memset(dacBuffer, DAC_MIDDLE, sampleCount);
        return sampleCount;
    }

    float phase = 0.0f;
    float phaseStep =
        (2.0f * PI_F * (float)frequency) / (float)SAMPLE_RATE;
    const uint32_t rampLength = SAMPLE_RATE / 200U;

    for (uint32_t i = 0; i < sampleCount; i++)
    {
        float envelope = 1.0f;

        if (i < rampLength)
        {
            envelope = (float)i / (float)rampLength;
        }
        else if ((sampleCount - i) <= rampLength)
        {
            envelope =
                (float)(sampleCount - i) / (float)rampLength;
        }

        float signal =
            (float)DAC_MIDDLE +
            DAC_AMPLITUDE * envelope * sinf(phase);

        if (signal > 255.0f)
        {
            signal = 255.0f;
        }
        else if (signal < 0.0f)
        {
            signal = 0.0f;
        }

        dacBuffer[i] = (uint8_t)signal;

        phase += phaseStep;

        if (phase >= (2.0f * PI_F))
        {
            phase -= 2.0f * PI_F;
        }
    }

    return sampleCount;
}

static void PlayFelizNavidadDAC(void)
{
    playing = 1;

    for (uint32_t note = 0; note < NOTE_COUNT; note++)
    {
        uint32_t sampleCount = GenerateNote(
            noteFrequency[note],
            noteDurationMs[note]
        );

        dacFinished = 0;
        __HAL_TIM_SET_COUNTER(&htim6, 0);

        if (HAL_DAC_Start_DMA(
                &hdac,
                DAC_CHANNEL_1,
                (uint32_t *)dacBuffer,
                sampleCount,
                DAC_ALIGN_8B_R) != HAL_OK)
        {
            Error_Handler();
        }

        if (HAL_TIM_Base_Start(&htim6) != HAL_OK)
        {
            Error_Handler();
        }

        while (dacFinished == 0)
        {
        }

        HAL_TIM_Base_Stop(&htim6);
        HAL_DAC_Stop_DMA(&hdac, DAC_CHANNEL_1);
    }

    HAL_DAC_Start(&hdac, DAC_CHANNEL_1);
    HAL_DAC_SetValue(
        &hdac,
        DAC_CHANNEL_1,
        DAC_ALIGN_8B_R,
        DAC_MIDDLE
    );

    playing = 0;
}

static void PlaySuperMarioPWM(void)
{
    playing = 1;

    if (HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_3) != HAL_OK)
    {
        Error_Handler();
    }

    for (uint32_t note = 0; note < MARIO_NOTE_COUNT; note++)
    {
        uint16_t frequency = marioFrequency[note];

        if (frequency == 0U)
        {
            __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_3, 0);
        }
        else
        {
            uint32_t period = PWM_COUNTER_HZ / frequency;

            __HAL_TIM_SET_AUTORELOAD(&htim2, period - 1U);
            __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_3, period / 2U);
            __HAL_TIM_SET_COUNTER(&htim2, 0);
            HAL_TIM_GenerateEvent(&htim2, TIM_EVENTSOURCE_UPDATE);
        }

        HAL_Delay(marioDurationMs[note]);
    }

    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_3, 0);
    HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_3);

    playing = 0;
}

static void ShowMenu(void)
{
    static const char menu[] =
        "\r\n=== Reproductor DAC/PWM ===\r\n"
        "1: Feliz Navidad por DAC\r\n"
        "2: Super Mario por PWM\r\n"
        "Seleccione una opcion: ";

    HAL_UART_Transmit(
        &huart2,
        (uint8_t *)menu,
        sizeof(menu) - 1U,
        HAL_MAX_DELAY
    );
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
  MX_DMA_Init();
  MX_DAC_Init();
  MX_TIM6_Init();
  MX_USART2_UART_Init();
  MX_TIM2_Init();
  /* USER CODE BEGIN 2 */
  HAL_DAC_Start(&hdac, DAC_CHANNEL_1);

  HAL_DAC_SetValue(
      &hdac,
      DAC_CHANNEL_1,
      DAC_ALIGN_8B_R,
      DAC_MIDDLE
  );

  ShowMenu();

  HAL_UART_Receive_IT(&huart2, &uartRx, 1);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    if (playRequest == 1)
    {
      playRequest = 0;

      PlayFelizNavidadDAC();

      HAL_UART_Transmit(
          &huart2,
          (uint8_t *)"\r\nReproduccion DAC terminada.\r\n",
          sizeof("\r\nReproduccion DAC terminada.\r\n") - 1U,
          HAL_MAX_DELAY
      );

      ShowMenu();
    }
    else if (playRequest == 2)
    {
      playRequest = 0;

      PlaySuperMarioPWM();

      HAL_UART_Transmit(
          &huart2,
          (uint8_t *)"\r\nReproduccion PWM terminada.\r\n",
          sizeof("\r\nReproduccion PWM terminada.\r\n") - 1U,
          HAL_MAX_DELAY
      );

      ShowMenu();
    }
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
  * @brief DAC Initialization Function
  * @param None
  * @retval None
  */
static void MX_DAC_Init(void)
{

  /* USER CODE BEGIN DAC_Init 0 */

  /* USER CODE END DAC_Init 0 */

  DAC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN DAC_Init 1 */

  /* USER CODE END DAC_Init 1 */

  /** DAC Initialization
  */
  hdac.Instance = DAC;
  if (HAL_DAC_Init(&hdac) != HAL_OK)
  {
    Error_Handler();
  }

  /** DAC channel OUT1 config
  */
  sConfig.DAC_Trigger = DAC_TRIGGER_T6_TRGO;
  sConfig.DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE;
  if (HAL_DAC_ConfigChannel(&hdac, &sConfig, DAC_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN DAC_Init 2 */

  /* USER CODE END DAC_Init 2 */

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 83;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 999;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */
  HAL_TIM_MspPostInit(&htim2);

}

/**
  * @brief TIM6 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM6_Init(void)
{

  /* USER CODE BEGIN TIM6_Init 0 */

  /* USER CODE END TIM6_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM6_Init 1 */

  /* USER CODE END TIM6_Init 1 */
  htim6.Instance = TIM6;
  htim6.Init.Prescaler = 0;
  htim6.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim6.Init.Period = 10499;
  htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim6) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim6, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM6_Init 2 */

  /* USER CODE END TIM6_Init 2 */

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
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Stream5_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream5_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream5_IRQn);

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
  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LD2_Pin */
  GPIO_InitStruct.Pin = LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2)
    {
        if (playing == 0)
        {
            if (uartRx == '1')
            {
                playRequest = 1;
            }
            else if (uartRx == '2')
            {
                playRequest = 2;
            }
        }

        HAL_UART_Receive_IT(&huart2, &uartRx, 1);
    }
}

void HAL_DAC_ConvCpltCallbackCh1(DAC_HandleTypeDef *hdacCallback)
{
    if (hdacCallback == &hdac)
    {
        dacFinished = 1;
    }
}

void HAL_DAC_ErrorCallbackCh1(DAC_HandleTypeDef *hdacCallback)
{
    if (hdacCallback == &hdac)
    {
        dacFinished = 1;
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
