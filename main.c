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
#include <stdio.h>
#include <string.h>
#include "lcd_i2c_bb.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum {
    STATE_STOP = 0,
    STATE_RUN_CW,
    STATE_RUN_CCW
} SystemState_t;

typedef enum {
    MODE_FULL_STEP = 1,
    MODE_HALF_STEP = 2
} StepperMode_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define STEPPER_PORT GPIOA
#define SEGMENT_PORT GPIOB
#define BUTTON_PORT  GPIOB
#define SEG_PORT GPIOB

#define SEG_A_PIN GPIO_PIN_9
#define SEG_B_PIN GPIO_PIN_10
#define SEG_C_PIN GPIO_PIN_11
#define SEG_D_PIN GPIO_PIN_12
#define SEG_E_PIN GPIO_PIN_13
#define SEG_F_PIN GPIO_PIN_14
#define SEG_G_PIN GPIO_PIN_15
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

TIM_HandleTypeDef htim2;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */
SystemState_t sys_state = STATE_STOP;
StepperMode_t sys_mode  = MODE_FULL_STEP;

uint16_t adc_value   = 0;
uint8_t  speed_level = 1;     // Range: 1 to 14
uint32_t step_delay  = 15;    // Milliseconds between steps
uint8_t  step_index  = 0;

uint32_t last_ui_update_tick = 0; // For non-blocking UI update

const uint8_t full_step_seq[4] = {0x0C, 0x06, 0x03, 0x09}; 
const uint8_t half_step_seq[8] = {0x08, 0x0C, 0x04, 0x06, 0x02, 0x03, 0x01, 0x09};

const uint8_t seg_code[10] = {
    0xC0, // 0: 1100 0000 (
    0xF9, // 1: 1111 1001 
    0xA4  // 2: 1010 0100
};
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_TIM2_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */
void Stepper_SetOutput(uint8_t pattern);
void Display_7Segment(uint8_t value);
void Update_UI_Interfaces(void);
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
  MX_ADC1_Init();
  MX_TIM2_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  LCD_Init();
  HAL_TIM_Base_Start_IT(&htim2);
  HAL_ADC_Start(&hadc1);
  Display_7Segment(0);
	
	char msg[] = "System Ready!\r\n";
  HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), 100);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	
  while (1)
  {

    // Process Button Inputs (Non-blocking debounce simulation)
    if (HAL_GPIO_ReadPin(BUTTON_PORT, BTN_STOP_Pin) == GPIO_PIN_RESET) {
        sys_state = STATE_STOP;
        HAL_Delay(150); // Simple debounce
    }
    else if (HAL_GPIO_ReadPin(BUTTON_PORT, BTN_CW_Pin) == GPIO_PIN_RESET) {
        sys_state = STATE_RUN_CW;
        HAL_Delay(150);
    }
    else if (HAL_GPIO_ReadPin(BUTTON_PORT, BTN_CCW_Pin) == GPIO_PIN_RESET) {
        sys_state = STATE_RUN_CCW;
        HAL_Delay(150);
    }
    else if (HAL_GPIO_ReadPin(BUTTON_PORT, BTN_FULL_Pin) == GPIO_PIN_RESET) {
        sys_mode = MODE_FULL_STEP;
        step_index = 0; // Reset index to prevent overflow
        HAL_Delay(150);
    }
    else if (HAL_GPIO_ReadPin(BUTTON_PORT, BTN_HALF_Pin) == GPIO_PIN_RESET) {
        sys_mode = MODE_HALF_STEP;
        step_index = 0;
        HAL_Delay(150);
    }

		// Process ADC and map to 14 speed levels
		if (HAL_GetTick() - last_ui_update_tick >= 200) {
        
        HAL_ADC_Start(&hadc1);
        if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK) {
            adc_value = HAL_ADC_GetValue(&hadc1);
            speed_level = (adc_value * 14) / 4095; 
            
            if (speed_level == 0) {
                step_delay = 0xFFFF; 
                Stepper_SetOutput(0x00);
            } else {
                step_delay = 56 - (speed_level * 3);
            }
        }

        Update_UI_Interfaces();
        
        last_ui_update_tick = HAL_GetTick();
    }
    
		HAL_Delay(10);
		
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
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
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
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Common config
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.ContinuousConvMode = ENABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_4;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

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

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 71;
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
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
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
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

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
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, STEP_IN1_Pin|STEP_IN2_Pin|STEP_IN3_Pin|STEP_IN4_Pin
                          |LCD_RS_Pin|LCD_EN_Pin|LCD_D4_Pin|LCD_D5_Pin
                          |LCD_D6_Pin|LCD_D7_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, SEG_B_Pin|SEG_C_Pin|SEG_D_Pin|SEG_E_Pin
                          |SEG_F_Pin|SEG_G_Pin|SEG_A_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : STEP_IN1_Pin STEP_IN2_Pin STEP_IN3_Pin STEP_IN4_Pin
                           LCD_RS_Pin LCD_EN_Pin LCD_D4_Pin LCD_D5_Pin
                           LCD_D6_Pin LCD_D7_Pin */
  GPIO_InitStruct.Pin = STEP_IN1_Pin|STEP_IN2_Pin|STEP_IN3_Pin|STEP_IN4_Pin
                          |LCD_RS_Pin|LCD_EN_Pin|LCD_D4_Pin|LCD_D5_Pin
                          |LCD_D6_Pin|LCD_D7_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : BTN_FULL_Pin BTN_HALF_Pin BTN_STOP_Pin BTN_CW_Pin
                           BTN_CCW_Pin */
  GPIO_InitStruct.Pin = BTN_FULL_Pin|BTN_HALF_Pin|BTN_STOP_Pin|BTN_CW_Pin
                          |BTN_CCW_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : SEG_B_Pin SEG_C_Pin SEG_D_Pin SEG_E_Pin
                           SEG_F_Pin SEG_G_Pin SEG_A_Pin */
  GPIO_InitStruct.Pin = SEG_B_Pin|SEG_C_Pin|SEG_D_Pin|SEG_E_Pin
                          |SEG_F_Pin|SEG_G_Pin|SEG_A_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void Stepper_SetOutput(uint8_t pattern) {
    HAL_GPIO_WritePin(STEPPER_PORT, STEP_IN1_Pin, (pattern & 0x01) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(STEPPER_PORT, STEP_IN2_Pin, (pattern & 0x02) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(STEPPER_PORT, STEP_IN3_Pin, (pattern & 0x04) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(STEPPER_PORT, STEP_IN4_Pin, (pattern & 0x08) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void Display_7Segment(uint8_t number) {
    if (number > 9) return; 

    uint8_t code = seg_code[number];

    HAL_GPIO_WritePin(SEG_PORT, SEG_A_PIN, (code & 0x01) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(SEG_PORT, SEG_B_PIN, (code & 0x02) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(SEG_PORT, SEG_C_PIN, (code & 0x04) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(SEG_PORT, SEG_D_PIN, (code & 0x08) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(SEG_PORT, SEG_E_PIN, (code & 0x10) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(SEG_PORT, SEG_F_PIN, (code & 0x20) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(SEG_PORT, SEG_G_PIN, (code & 0x40) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void Update_UI_Interfaces(void) {
    char lcd_buffer_line1[17];
    char lcd_buffer_line2[17];
    char uart_buffer[64];

    // 1. Update 7-Segment Display
    if (sys_state == STATE_STOP) {
        Display_7Segment(0);
    } else {
        Display_7Segment((sys_mode == MODE_FULL_STEP) ? 1 : 2);
    }

    // 2. Update LCD 16x2
    sprintf(lcd_buffer_line1, "Mode: %s", (sys_mode == MODE_FULL_STEP) ? "FULL" : "HALF");
    LCD_Print_Line(0, lcd_buffer_line1);

    if (sys_state == STATE_STOP) {
        sprintf(lcd_buffer_line2, "STOP");
    } else {
        sprintf(lcd_buffer_line2, "%s Spd: %02d", (sys_state == STATE_RUN_CW) ? "CW " : "CCW", speed_level);
    }
    LCD_Print_Line(1, lcd_buffer_line2);

    // 3. Update UART Terminal
		static uint32_t last_uart_tick = 0; 
    
    if (HAL_GetTick() - last_uart_tick >= 2000) {
        char uart_buffer[64];
			  if (sys_state == STATE_STOP) {
					  sprintf(uart_buffer, "Mode: %s | Stat: STOP\r\n\r\n", 
		        (sys_mode == MODE_FULL_STEP) ? "FULL" : "HALF");
            HAL_UART_Transmit(&huart1, (uint8_t*)uart_buffer, strlen(uart_buffer), 10);
				}
		    else{
						sprintf(uart_buffer, "Mode: %s | Stat: %s | Spd: %02d | ADC: %04d\r\n\r\n", 
						(sys_mode == MODE_FULL_STEP) ? "FULL" : "HALF", (sys_state == STATE_RUN_CW) ? "CW " : "CCW", speed_level, adc_value);
						HAL_UART_Transmit(&huart1, (uint8_t*)uart_buffer, strlen(uart_buffer), 10);
				}
				
				last_uart_tick = HAL_GetTick();
		}
				
}

// Timer Interrupt Callback for Stepper Control
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    static uint32_t tick_counter = 0;

    if (htim->Instance == TIM2) {
        if (sys_state == STATE_STOP || step_delay == 0xFFFF) {
            Stepper_SetOutput(0x00); // Release motor holding torque
            return;
        }

        tick_counter++;
        if (tick_counter >= step_delay) {
            tick_counter = 0;

            if (sys_mode == MODE_FULL_STEP) {
                Stepper_SetOutput(full_step_seq[step_index]);
                if (sys_state == STATE_RUN_CW) step_index = (step_index + 1) % 4;
                else step_index = (step_index == 0) ? 3 : step_index - 1;
            } else {
                Stepper_SetOutput(half_step_seq[step_index]);
                if (sys_state == STATE_RUN_CW) step_index = (step_index + 1) % 8;
                else step_index = (step_index == 0) ? 7 : step_index - 1;
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
