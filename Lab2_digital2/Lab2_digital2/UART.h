/*
 * UART.h
 *
 * Created: 
 * Author: 
 * Description: Librería UART para ATMega328P
 */

#ifndef UART_H_
#define UART_H_

void initUART(void);
void writeChar(char caracter);
void writeString(char* string);

#endif /* UART_H_ */