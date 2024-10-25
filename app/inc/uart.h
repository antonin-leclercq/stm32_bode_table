/*
 * uart.h
 *
 *  Created on: Sep 1, 2024
 *      Author: anton
 */

#ifndef APP_INC_UART_H_
#define APP_INC_UART_H_

#define UART_RX_INT_PRIORITY 15

void UART_Init(void);
void UART_NVIC_Init(void);
void UART_RXINT_Enable(void);
void UART_RXINT_Disable(void);

#endif /* APP_INC_UART_H_ */
