/*
 * main.c
 *
 *  Created on: Sep 1, 2024
 *      Author: anton
 */

#include "uart.h"
#include "timer.h"
#include "adc.h"
#include "stm32f0xx.h"

extern int stm32_printf(const char *format, ...);
extern int stm32_sprintf(char *out, const char *format, ...);

// declared in timer.c
extern uint16_t timer_cnt[TIMER_CNT_SIZE];

// declared in adc.c
extern uint16_t adc_max_data[ADC_MAX_DATA_SIZE];

enum Menu_State
{
	ROOT = 0,
	TIMER_CONF,
	ADC_CONF,
	INPUT_TIM_PSC, // for entering numbers
	INPUT_ADC_SMP,
	INPUT_TIM_FDIV,
	ACQ_DONE,
	ACQ_RUNNING,
};

struct Input_Number
{
	uint8_t buffer[4];
	uint32_t value;
	uint32_t pow_10;
	uint32_t n_digits;
};

static void SystemClock_Config(void);
static void Print_Root_Menu(void);
static void Print_Timer_Menu(void);
static void Print_ADC_Menu(void);
static uint8_t is_number(const uint8_t ascii);
static void Clear_Input_Number(struct Input_Number* in);
static enum Menu_State Get_Input(uint8_t key, struct Input_Number* in);
static void Print_ACQ_DONE_Info(void);

enum Menu_State menu_state = ROOT;

// Variables modified by ISRs
volatile uint8_t current_key = 0;
volatile uint8_t is_new_key = 1;
volatile uint8_t adc_acq_data_filled = 0;
volatile uint8_t error_flag = 0;

const uint8_t adc_smp[] = {1, 7, 13, 28, 41, 55, 71, 239};

int main(void)
{
	// Switch to 48MHz SYSCLK
	SystemClock_Config();

	// Configure peripherals
	UART_Init();
	stm32_printf("UART initialized\r\n");

	TIMER_FDIV_Init();
	TIMER_IC_Init();
	stm32_printf("TIMER initialized\r\n");

	ADC_Init();
	stm32_printf("ADC initialized and calibrated\r\n");

	// Enable interrupts
	UART_NVIC_Init();
	TIMER_FDIV_NVIC_Init();
	TIMER_IC_NVIC_Init();

	struct Input_Number input_number;
	Clear_Input_Number(&input_number);

	uint8_t acq_running = 1;

	while(1)
	{
		// Check for general errors
		if (error_flag)
		{
			stm32_printf("\r\nProgram encountered an error. Please hard-reset the CPU ");
			while(1){}
		}
		// flag set when all ADC data for 1 max sample is collected
		if (adc_acq_data_filled)
		{
			// Stop ADC
			ADC_ACQ_Disable();

			// calculate max ADC value and copy to buffer
			acq_running = ADC_Update_Max_Data();

			// Enable ADC for next samples
			ADC_ACQ_Enable();

			// clear flag
			adc_acq_data_filled = 0;
		}

		// flag is zero when total acquisition is completed without errors
		if (acq_running == 0)
		{
			menu_state = ACQ_DONE;

			TIMER_IC_ACQ_Disable();
			ADC_ACQ_Disable();

			// Change flag so we do not execute this portion over and over again
			acq_running = 1;
		}
		else if (is_new_key == 0)
		{
			continue;
		}

		is_new_key = 0;

		// Parse input
		switch(menu_state)
		{
		case ROOT:
			if(current_key == 't') menu_state = TIMER_CONF;
			else if(current_key == 'a') menu_state = ADC_CONF;
			else if(current_key == 's')
			{
				UART_RXINT_Disable();
				TIMER_IC_ACQ_Enable();
				ADC_ACQ_Enable();

				menu_state = ACQ_RUNNING;
			}
			break;
		case TIMER_CONF:
			if(current_key == 'r') menu_state = ROOT;
			else if(current_key == 'p')
			{
				menu_state = INPUT_TIM_PSC;
				stm32_printf("\r\nEnter TIM1->PSC followed by <ENTER>: ");
			}
			else if (current_key == 'f')
			{
				menu_state = INPUT_TIM_FDIV;
				stm32_printf("\r\nEnter TIM15->ARR followed by <ENTER>: ");
			}
			break;
		case ADC_CONF:
			if(current_key == 'r') menu_state = ROOT;
			else if(current_key == 's')
			{
				menu_state = INPUT_ADC_SMP;
				stm32_printf("\r\n");
				for(int i = 0; i < 8; ++i)
				{
					stm32_printf("%d for %d,5 ADC clock cycles\r\n", i, adc_smp[i]);
				}
				stm32_printf("\r\nEnter correct number for ADC1->SMPR: ");
			}
			break;
		case INPUT_TIM_PSC:
			if(Get_Input(current_key, &input_number))
			{
				menu_state = ROOT;
				TIMER_IC_Set_PSC((uint16_t) input_number.value);
				Clear_Input_Number(&input_number);
			}
			break;
		case INPUT_ADC_SMP:
			int smpr = current_key - '0';
			if (smpr >= 0 && smpr <= 7)
			{
				ADC_Set_SMPR((uint8_t) smpr);
			}
			else
			{
				stm32_printf("[ERROR]: value out of range\r\n");
			}
			menu_state = ROOT;
			break;
		case INPUT_TIM_FDIV:
			if(Get_Input(current_key, &input_number))
			{
				menu_state = ROOT;
				TIMER_FDIV_Set_CNT((uint16_t) input_number.value);
				Clear_Input_Number(&input_number);
			}
			break;
		default:
			break;
		}

		// Print menus
		switch(menu_state)
		{
		case ROOT:
			Print_Root_Menu();
			break;
		case TIMER_CONF:
			Print_Timer_Menu();
			break;
		case ADC_CONF:
			Print_ADC_Menu();
			break;
		case ACQ_DONE:
			Print_ACQ_DONE_Info();
			UART_RXINT_Enable();

			// Set flag to print menu
			menu_state = ROOT;
			is_new_key = 1;
			current_key = 0;
			break;
		case ACQ_RUNNING:
			stm32_printf("\r\nRunning acquisition...\r\n");
			break;
		default:
			break;
		}
	}

	return 0;
}

void SystemClock_Config(void)
{
	uint32_t HSE_Status = 0;
	uint32_t PLL_Status = 0;
	uint32_t SW_Status = 0;
	uint32_t timeout = 100000;

	// Start HSE in Bypass Mode
	RCC->CR |= RCC_CR_HSEBYP;
	RCC->CR |= RCC_CR_HSEON;

	// Wait until HSE is ready
	do
	{
		HSE_Status = RCC->CR & RCC_CR_HSERDY_Msk;
		timeout--;
	} while ((HSE_Status == 0) && (timeout > 0));

	// Select HSE as PLL input source
	RCC->CFGR &= ~RCC_CFGR_PLLSRC_Msk;
	RCC->CFGR |= (0x02 << RCC_CFGR_PLLSRC_Pos);

	// Set PLL PREDIV to /1
	RCC->CFGR2 = 0x00000000;

	// Set PLL MUL to x6 --> fPLL = 8MHz * 6 = 48MHz (max)
	RCC->CFGR &= ~RCC_CFGR_PLLMUL_Msk;
	RCC->CFGR |= (0x04 << RCC_CFGR_PLLMUL_Pos);

	// Enable the main PLL
	RCC-> CR |= RCC_CR_PLLON;

	// Wait until PLL is ready
	do
	{
		PLL_Status = RCC->CR & RCC_CR_PLLRDY_Msk;
		timeout--;
	} while ((PLL_Status == 0) && (timeout > 0));

        // Set AHB prescaler to /1
	RCC->CFGR &= ~RCC_CFGR_HPRE_Msk;
	RCC->CFGR |= RCC_CFGR_HPRE_DIV1;

	//Set APB1 prescaler to /1
	RCC->CFGR &= ~RCC_CFGR_PPRE_Msk;
	RCC->CFGR |= RCC_CFGR_PPRE_DIV1;

	// Enable FLASH Prefetch Buffer and set Flash Latency
	FLASH->ACR = FLASH_ACR_PRFTBE | FLASH_ACR_LATENCY;

	// Select the main PLL as system clock source
	RCC->CFGR &= ~RCC_CFGR_SW;
	RCC->CFGR |= RCC_CFGR_SW_PLL;

	// Wait until PLL becomes main switch input
	do
	{
		SW_Status = (RCC->CFGR & RCC_CFGR_SWS_Msk);
		timeout--;
	} while ((SW_Status != RCC_CFGR_SWS_PLL) && (timeout > 0));

	// Update SystemCoreClock global variable
	SystemCoreClockUpdate();
}

static void Print_Root_Menu(void)
{
	static const char* root_menu_str = "t to view / change TIMER configuration\r\n"
									   "a to view / change ADC configuration\r\n"
									   "s to start acquisition\r\n";
	stm32_printf("\r\nEnter one of the following keys:\r\n%s=>>", root_menu_str);
}

static void Print_Timer_Menu(void)
{
#define TIMER_FREQ 48e6
	const uint16_t psc = TIM1->PSC + 1;
	const float f_float = (float) TIMER_FREQ / psc;
	const uint32_t f_int = (uint32_t) f_float;
	const uint16_t div_by = (uint16_t) TIM15->ARR;

	// Multiply difference by 100 to have two decimal places
	const uint32_t f_dec = (f_float - (float) f_int) * 100.f;
	stm32_printf("\r\n[TIMER CONFIG]:\r\nPrescaler=%d\r\nCounter frequency=%d,%d Hz\r\nDivide-by=%d\r\n", psc, f_int, f_dec, div_by);

	static const char* timer_menu_str = "p to change TIMER1 pre-scaler\r\n"
										"f to change TIMER15 auto-reload value (=divide-by)\r\n"
										"r to go back to ROOT menu\r\n";
	stm32_printf("Enter one of the following keys:\r\n%s=>>", timer_menu_str);
}

static void Print_ADC_Menu(void)
{
#define ADC_FREQ 24e6
	uint8_t smpr_int = 0;
	uint8_t index = ADC1->SMPR & 0x07;
	smpr_int = adc_smp[index];

	stm32_printf("\r\n[ADC CONFIG]:\r\nSampling time=%d,5 clock cycles\r\n", smpr_int);

	static const char* timer_menu_str = "s to change ADC sampling time\r\n"
										"r to go back to ROOT menu\r\n";
	stm32_printf("Enter one of the following keys:\r\n%s=>>", timer_menu_str);
}

static uint8_t is_number(const uint8_t ascii)
{
	return (0x30 <= ascii) && (ascii <= 0x39);
}

static void Clear_Input_Number(struct Input_Number* in)
{
	in->buffer[0] = 0;
	in->buffer[0] = 0;
	in->buffer[0] = 0;
	in->buffer[0] = 0;
	in->value = 0;
	in->pow_10 = 1;
	in->n_digits = 0;
}

static uint8_t Get_Input(uint8_t key, struct Input_Number* in)
{
	uint8_t done = 0;
	if(key == '\r')
	{
		// Build the final number
		for (int j = in->n_digits -1; j >= 0; --j)
		{
			in->value += in->buffer[j] * in->pow_10;
			in->pow_10 *= 10;
		}

		done = 1;
	}
	else if(is_number(key) && in->n_digits < 4)
	{
		// Save the digits
		in->buffer[in->n_digits] = (key - '0');
		++in->n_digits;
	}

	return done;
}

static void Print_ACQ_DONE_Info(void)
{
	stm32_printf("\r\n[INFO]: Acquisition complete\r\n");
	stm32_printf("Raw TIM1->CNT data :\r\n[");
	for (uint32_t i = 0; i < TIMER_CNT_SIZE; ++i)
	{
		stm32_printf("%d, ", timer_cnt[i]);
		if (i+1 % 16 == 0)
		{
			stm32_printf("\r\n");
		}
	}
	stm32_printf("]\r\n");

	stm32_printf("\r\nRaw ADC->DR data :\r\n[");
	for (uint32_t i = 0; i < ADC_MAX_DATA_SIZE; ++i)
	{
		stm32_printf("%d, ", adc_max_data[i]);
		if (i+1 % 16 == 0)
		{
			stm32_printf("\r\n");
		}
	}
	stm32_printf("]\r\n");
}
