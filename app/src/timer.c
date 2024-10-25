/*
 * timer.c
 *
 *  Created on: Sep 1, 2024
 *      Author: anton
 */

#include "timer.h"
#include "stm32f0xx.h"

// data buffer
uint16_t timer_cnt[TIMER_CNT_SIZE] = {0};

uint16_t previous_divide_by = 100;

void TIMER_IC_Init(void)
{
	// TIM1 in input capture mode on TI1, connected to PA8
	// TIM1 triggered when TIM15 counter reaches to divide-by value
	// APB2 peripheral frequency is 48MHz
	// using DMA (Channel 2) to save counter data in memory

///////////////////////////////////////////////////////// GPIO Config

	// Enable GPIOB clock
	RCC->AHBENR |= RCC_AHBENR_GPIOAEN;

	// GPIO configuration: PA8 in AF2
	GPIOA->MODER &= ~GPIO_MODER_MODER8_Msk;
	GPIOA->MODER |= (0x02 << GPIO_MODER_MODER8_Pos);
	GPIOA->AFR[1] &= ~GPIO_AFRH_AFRH0_Msk;
	GPIOA->AFR[1] |= (0x02 << GPIO_AFRH_AFRH0_Pos);

///////////////////////////////////////////////////////// TIM1 Config

	// Enable TIM1 clock
	RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;

	// Reset TIM1 configuration
	TIM1->CR1 = 0x0000;
	TIM1->CR2 = 0x0000;

	// Set TIM1 pre-scaler to /48 to count every 1us
	TIM1->PSC = (uint16_t) 48 -1;

	// Set ARR to max value
	TIM1->ARR = (uint16_t) 0xFFFF;

	// Select internal trigger input to TIM15 (divide-by counter) => ITR0
	TIM1->SMCR &= ~TIM_SMCR_TS_Msk;
	TIM1->SMCR |= (0x00 << TIM_SMCR_TS_Pos);

	// Set slave mode to reset counter (rising edge on ITR0 resets counter and starts it)
	TIM1->SMCR &= ~TIM_SMCR_SMS_Msk;
	TIM1->SMCR |= (0x04 << TIM_SMCR_SMS_Pos);

	// Enable master/slave to have better sync between TIM15_TRGO => TIM1_ITR0
	TIM1->SMCR |= TIM_SMCR_MSM;

	// Enable CH1 compare interrupt
	TIM1->DIER |= TIM_DIER_CC1IE;

	// Enable master mode to launch TRGO on reset of counter (compare pulse) event to start ADC
	TIM1->CR2 |= (0x00 << TIM_CR2_MMS_Pos);

	// Set CH1 as input, mapped on TI1
	TIM1->CCMR1 &= ~TIM_CCMR1_CC1S_Msk;
	TIM1->CCMR1 |= (0x01 << TIM_CCMR1_CC1S_Pos);

	// Input filter frequency at fDTS /2, N=6
	TIM1->CCMR1 &= ~TIM_CCMR1_IC1F_Msk;
	TIM1->CCMR1 |= (0x04 << TIM_CCMR1_IC1F_Pos);

	// Set TI1 sensitive to falling edge
	TIM1->CCER &= ~TIM_CCER_CC1P_Msk;
	TIM1->CCER |= (0x01 << TIM_CCER_CC1P_Pos);
}

void TIMER_IC_NVIC_Init(void)
{
	NVIC_SetPriority(TIM1_CC_IRQn, TIM1_CC_INT_PRIORITY);
	NVIC_EnableIRQ(TIM1_CC_IRQn);
}

void TIMER_IC_ACQ_Enable(void)
{
	// Enable TIM1 channel 1
	TIM1->CCER |= TIM_CCER_CC1E;

	// Enable TIM1 counter
	TIM1->CR1 |= TIM_CR1_CEN;
}

void TIMER_IC_ACQ_Disable(void)
{
	// Disable TIM1 counter
	TIM1->CR1 &= ~TIM_CR1_CEN;

	// Disable CH1
	TIM1->CCER &= ~TIM_CCER_CC1E_Msk;
}

inline void TIMER_IC_Set_PSC(const uint16_t psc)
{
	TIM1->PSC = (uint16_t) psc -1;
}

void TIMER_IC_Data_Update(const uint16_t new_capture)
{
	static uint32_t i = 0;

	// Save current capture in buffer
	timer_cnt[i++] = new_capture;
	if(i >= TIMER_CNT_SIZE) i = 0;
}

void TIMER_FDIV_Init(void)
{
	// TIM15 as frequency divider (counter externally clocked)
	// Generates an interrupt when counter overflows
	// Signal input on PB14 as AF1 (TIM15 CH1)

///////////////////////////////////////////////////////// GPIO Config

	// Enable GPIOB clock
	RCC->AHBENR |= RCC_AHBENR_GPIOBEN;

	// Set PB14 to AF1
	GPIOB->MODER &= ~GPIO_MODER_MODER14_Msk;
	GPIOB->MODER |= (0x02 << GPIO_MODER_MODER14_Pos);
	GPIOB->AFR[1] &= ~GPIO_AFRH_AFRH6_Msk;
	GPIOB->AFR[1] |= (0x01 << GPIO_AFRH_AFRH6_Pos);

	// Enable GPIOA clock
	RCC->AHBENR |= RCC_AHBENR_GPIOAEN;

	// Set PA5 as output low speed (error LED)
	GPIOA->MODER &= ~GPIO_MODER_MODER5_Msk;
	GPIOA->MODER |= (0x01 << GPIO_MODER_MODER5_Pos);
	GPIOA->OSPEEDR &= ~GPIO_OSPEEDR_OSPEEDR5_Msk;
	GPIOA->ODR &= ~GPIO_ODR_5;

///////////////////////////////////////////////////////// TIM15 Config

	// Enable TIM15 clock
	RCC->APB2ENR |= RCC_APB2ENR_TIM15EN;

	// Reset TIM15 configuration
	TIM15->CR1 = 0x0000;
	TIM15->CR2 = 0x0000;

	// Enable auto-reload pre-load
	TIM15->CR1 |= TIM_CR1_ARPE;

	// Only counter over/underflow can generate interrupt
	TIM15->CR1 |= TIM_CR1_URS;

	// Use TI1FP1 as counter clock source
	TIM15->SMCR &= ~TIM_SMCR_TS_Msk;
	TIM15->SMCR |= (0x05 << TIM_SMCR_TS_Pos);

	// Enable external clock mode
	TIM15->SMCR &= ~TIM_SMCR_SMS_Msk;
	TIM15->SMCR |= (0x07 << TIM_SMCR_SMS_Pos);

	// Enable update interrupt (for counter overflow)
	TIM15->DIER |= TIM_DIER_UIE;

	// Set CH1 as input
	TIM15->CCMR1 &= ~TIM_CCMR1_CC1S_Msk;
	TIM15->CCMR1 |= (0x01 << TIM_CCMR1_CC1S_Pos);

	// Set input filter to fDTS /2 and N=6
	TIM15->CCMR1 &= ~TIM_CCMR1_IC1F_Msk;
	TIM15->CCMR1 |= (0x04 << TIM_CCMR1_IC1F_Pos);

	// Do not divide external input clock
	TIM15->PSC = (uint16_t) 0;

	// Set auto-reload to default
	TIM15->ARR = (uint16_t) 0xFFFF;

	// Set counter default value
	TIM15->CNT = (uint16_t) 0xFFFF -previous_divide_by;

	// Enable counter
	TIM15->CR1 |= TIM_CR1_CEN;
}

void TIMER_FDIV_NVIC_Init(void)
{
	NVIC_SetPriority(TIM15_IRQn, TIM15_OVF_INT_PRIORITY);
	NVIC_EnableIRQ(TIM15_IRQn);
}

inline void TIMER_FDIV_Set_CNT(const uint16_t arr)
{
	previous_divide_by = arr;
	TIM15->CNT = (uint16_t) 0xFFFF - arr;
}

inline void TIMER_FDIV_Disable(void)
{
	TIM15->CR1 &= ~TIM_CR1_CEN;
}
