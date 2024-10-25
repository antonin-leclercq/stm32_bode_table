/*
 * adc.c
 *
 *  Created on: Sep 1, 2024
 *      Author: anton
 */

#include "adc.h"
#include "stm32f0xx.h"

// data buffer
uint16_t adc_max_data[ADC_MAX_DATA_SIZE] = {0};
// This buffer contains the digitized half sine wave, and should not be totally filled up
uint16_t adc_acq_data[ADC_ACQ_DATA_SIZE] = {0};

void ADC_Init(void)
{
	// ADC input: PB0 (ADC_IN8)
	// ADC running from PCLK /2 => 24MHz
	// External conversion triggering by TIM15_TRGO
	// Using DMA channel 1 to copy acquisition data

///////////////////////////////////////////////////////// GPIO Config

	// Enable GPIOB clock
	RCC->AHBENR |= RCC_AHBENR_GPIOBEN;

	// Configure PB0 as analog mode
	GPIOB->MODER &= ~GPIO_MODER_MODER0_Msk;
	GPIOB->MODER |= (0x03 << GPIO_MODER_MODER0_Pos);

///////////////////////////////////////////////////////// DMA Config

	// Enable DMA1 clock
	RCC->AHBENR |= RCC_AHBENR_DMA1EN;

	// Reset configuration
	DMA1_Channel1->CCR = 0x00000000;

	// Set channel priority to high
	DMA1_Channel1->CCR |= (0x02 << DMA_CCR_PL_Pos);

	// Set memory data size to 16 bits
	DMA1_Channel1->CCR |= (0x01 << DMA_CCR_MSIZE_Pos);

	// Set peripheral data size to 16 bits
	DMA1_Channel1->CCR |= (0x01 << DMA_CCR_PSIZE_Pos);

	// Enable memory increment mode
	DMA1_Channel1->CCR |= DMA_CCR_MINC;

	// Set number of data register (= size of adc_acq_data)
	DMA1_Channel1->CNDTR = (uint16_t) ADC_ACQ_DATA_SIZE;

	// Set peripheral address to TIM15->CNT
	DMA1_Channel1->CPAR = (uint32_t) &ADC1->DR;

	// Set memory base address to adc_data
	DMA1_Channel1->CMAR = (uint32_t) adc_acq_data;

///////////////////////////////////////////////////////// ADC1 Config

	// Enable ADC clock
	RCC->APB2ENR |= RCC_APB2ENR_ADCEN;

	// Reset ADC configuration
	ADC1->CR = 0x00000000;
	ADC1->CFGR1 = 0x00000000;
	ADC1->CFGR2 = 0x00000000;

	// Set clock input to PCLK /2
	ADC1->CFGR2 |= (0x01 << ADC_CFGR2_CKMODE_Pos);

	// Calibrate ADC
	ADC1->CR |= ADC_CR_ADCAL;

	// Wait for calibration to complete
	while((ADC1->CR & ADC_CR_ADCAL) != ADC_CR_ADCAL);

	// Set ADC resolution to 10 bits
	ADC1->CFGR1 &= ~ADC_CFGR1_RES_Msk;
	ADC1->CFGR1 |= (0x01 << ADC_CFGR1_RES_Pos);

	// Set ADC sampling time to 28.5 clock cycles (=? us)
	ADC1->SMPR &= ~ADC_SMPR_SMP_Msk;
	ADC1->SMPR |= (0x03 << ADC_SMPR_SMP_Pos);

	// Select channel 8 for conversion
	ADC1->CHSELR = ADC_CHSELR_CHSEL8;

	// Enable external triggering on rising edge
	ADC1->CFGR1 &= ~ADC_CFGR1_EXTEN_Msk;
	ADC1->CFGR1 |= (0x01 << ADC_CFGR1_EXTEN_Pos);

	// Select TRG4 (TIM15_TRGO as external trigger)
	ADC1->CFGR1 &= ~ADC_CFGR1_EXTSEL_Msk;
	ADC1->CFGR1 |= (0x04 << ADC_CFGR1_EXTSEL_Pos);

	// Enable continuous conversion mode (1 trigger from TRGO starts continuous conversion)
	ADC1->CFGR1 |= ADC_CFGR1_CONT;

	// Enable DMA requests
	ADC1->CFGR1 |= ADC_CFGR1_DMAEN;
}

void ADC_ACQ_Enable(void)
{
	// Set number of data register (= size of adc_data)
	DMA1_Channel1->CNDTR = (uint16_t) ADC_ACQ_DATA_SIZE;

	// Enable DMA CH1 for ADC
	DMA1_Channel1->CCR |= DMA_CCR_EN;

	// Enable ADC
	ADC1->CR |= ADC_CR_ADEN;

	// Start conversion (on trigger)
	ADC1->CR |= ADC_CR_ADSTART;
}

void ADC_ACQ_Disable(void)
{
	// wait for DMA to be done with ADC (should be done at the same time)
	while((DMA1->ISR & DMA_ISR_TCIF1) != DMA_ISR_TCIF1);

	// Stop ADC conversion
	ADC1->CR |= ADC_CR_ADSTP;
	while((ADC1->CR & ADC_CR_ADSTP) == ADC_CR_ADSTP);

	// Stop ADC
	ADC1->CR |= ADC_CR_ADDIS;
	while((ADC1->CR & ADC_CR_ADDIS) == ADC_CR_ADDIS);

	// Disable DMA for ADC requests
	DMA1_Channel1->CCR &= ~DMA_CCR_EN;
}

inline void ADC_Set_SMPR(const uint8_t smpr)
{
	// Stop any ongoing ADC conversion
	ADC1->CR |= ADC_CR_ADSTP;
	while((ADC1->CR & ADC_CR_ADSTP) == ADC_CR_ADSTP);

	ADC1->SMPR = smpr;
}

uint16_t ADC_Find_Max_Value(void)
{
	uint16_t adc_max = 0;
	for (uint32_t i = 0; i < ADC_ACQ_DATA_SIZE; ++i)
	{
		if (adc_acq_data[i] >= adc_max)
		{
			adc_max = adc_acq_data[i];
		}
	}
	return adc_max;
}

uint8_t ADC_Update_Max_Data(void)
{
	static uint16_t n = 0;
	if (n < ADC_MAX_DATA_SIZE)
	{
		adc_max_data[n] = ADC_Find_Max_Value();
		++n;
	}
	else
	{
		n = 0;
	}
	// if n > 0: still filling buffer
	// if n = 0: done filling buffer
	return n;
}
