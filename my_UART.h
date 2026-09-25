#ifndef __my_UART_H
#define __my_UART_H


void UART1_Init(void);
void UART1_SendByte(uint8_t byte);
void UART1_SendString(const char* str);
uint8_t UART1_ReadByte(void);
int32_t UART1_ReadByteTimeout(uint32_t timeout_val);




#endif
