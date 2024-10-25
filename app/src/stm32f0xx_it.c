/**
  ******************************************************************************
  * @file    Templates/Src/stm32f0xx_it.c 
  * @author  MCD Application Team
  * @brief   Main Interrupt Service Routines.
  *          This file provides template for all exceptions handler and 
  *          peripherals interrupt service routine.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2016 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "stm32f0xx_it.h"
#include "stm32f0xx.h"
#include "timer.h"
#include "adc.h"


/** @addtogroup STM32F0xx_HAL_Examples
  * @{
  */

/** @addtogroup Templates
  * @{
  */

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/******************************************************************************/
/*            Cortex-M0 Processor Exceptions Handlers                         */
/******************************************************************************/

extern uint8_t current_key;
extern uint8_t is_new_key;
extern uint16_t previous_divide_by;
extern uint8_t adc_acq_data_filled;
extern uint8_t error_flag;

void USART2_IRQHandler(void)
{
	// Source is RX interrupt
	if ((USART2->ISR & USART_ISR_RXNE) == USART_ISR_RXNE)
	{
		// Reading data register clears interrupt
		current_key = (uint8_t) USART2->RDR;

		is_new_key = 1;

		// Echo received data
		while((USART2->ISR & USART_ISR_TXE) != USART_ISR_TXE);
		USART2->TDR = (uint16_t) current_key;
	}
}

void TIM1_CC_IRQHandler(void)
{
	// Source is TIM1 CC1IF (capture on falling edge on CH1)
	if ((TIM1->SR & TIM_SR_CC1IF) == TIM_SR_CC1IF)
	{
		// Read captured value
		TIMER_IC_Data_Update(TIM1->CCR1);

		// Clear interrupt
		TIM1->SR &= ~TIM_SR_CC1IF;

		// Set flag for main.c
		adc_acq_data_filled = 1;
	}
}

void TIM15_IRQHandler(void)
{
	// Source is CNT overflow (= update interrupt)
	// This triggers acquisition of new sample
	if ((TIM15->SR & TIM_SR_UIF) == TIM_SR_UIF)
	{
		// Clear interrupt
		TIM15->SR &= ~TIM_SR_UIF;

		// Reload counter
		TIM15->CNT = (uint16_t) 0xFFFF -previous_divide_by;

		// Check if CPU managed to handle all previous data, to avoid DMA overwriting
		if (adc_acq_data_filled == 1)
		{
			// Set LD2 connected on PA5
			GPIOA->ODR |= GPIO_ODR_5;

			// Disable ADC
			ADC_ACQ_Disable();

			// Disable timers, etc... so that when we go back to the main loop
			// the CPU still finishes its ADC task but stops and prints error message afterwards
			TIMER_IC_ACQ_Disable();
			TIMER_FDIV_Disable();

			error_flag = 1;
		}
	}
}

/**
  * @brief  This function handles NMI exception.
  * @param  None
  * @retval None
  */
void NMI_Handler(void)
{
}

/**
  * @brief  This function handles Hard Fault exception.
  * @param  None
  * @retval None
  */
void HardFault_Handler(void)
{
  /* Go to infinite loop when Hard Fault exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles SVCall exception.
  * @param  None
  * @retval None
  */
void SVC_Handler(void)
{
}

/**
  * @brief  This function handles PendSVC exception.
  * @param  None
  * @retval None
  */
void PendSV_Handler(void)
{
}

/******************************************************************************/
/*                 STM32F0xx Peripherals Interrupt Handlers                   */
/*  Add here the Interrupt Handler for the used peripheral(s) (PPP), for the  */
/*  available peripheral interrupt handler's name please refer to the startup */
/*  file (startup_stm32f0xx.s).                                               */
/******************************************************************************/

/**
  * @brief  This function handles PPP interrupt request.
  * @param  None
  * @retval None
  */
/*void PPP_IRQHandler(void)
{
}*/


/**
  * @}
  */ 

/**
  * @}
  */
