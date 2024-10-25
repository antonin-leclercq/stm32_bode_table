/*
 * adc.h
 *
 *  Created on: Sep 1, 2024
 *      Author: anton
 */

#ifndef APP_INC_ADC_H_
#define APP_INC_ADC_H_

#define ADC_MAX_DATA_SIZE 1024
#define ADC_ACQ_DATA_SIZE 256

void ADC_Init(void);
void ADC_ACQ_Enable(void);
void ADC_ACQ_Disable(void);
void ADC_Set_SMPR(const unsigned char smpr);
unsigned short ADC_Find_Max_Value(void);
unsigned char ADC_Update_Max_Data(void);

#endif /* APP_INC_ADC_H_ */
