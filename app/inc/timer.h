/*
 * timer.h
 *
 *  Created on: Sep 1, 2024
 *      Author: anton
 */

#ifndef APP_INC_TIMER_H_
#define APP_INC_TIMER_H_

#define TIM1_CC_INT_PRIORITY 10
#define TIM15_OVF_INT_PRIORITY 9

#define TIMER_CNT_SIZE 1024

void TIMER_IC_Init(void);
void TIMER_IC_NVIC_Init(void);
void TIMER_IC_ACQ_Enable(void);
void TIMER_IC_ACQ_Disable(void);
void TIMER_IC_Set_PSC(const unsigned short psc);
void TIMER_IC_Data_Update(const unsigned short new_capture);

void TIMER_FDIV_Init(void);
void TIMER_FDIV_NVIC_Init(void);
void TIMER_FDIV_Set_CNT(const unsigned short arr);
void TIMER_FDIV_Disable(void);

#endif /* APP_INC_TIMER_H_ */
