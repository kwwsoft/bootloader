#include "stm32f10x.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_usart.h"
#include "global.h"
#include "my_UART.h"




//******************************************************************
void UART1_Init(void) {
    // 1. Вмикаємо тактування для GPIOA, USART1 та альтернативних функцій (AFIO)
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_USART1 | RCC_APB2Periph_AFIO, ENABLE);

    GPIO_InitTypeDef GPIO_InitStructure;

    // 2. Налаштовуємо ніжку TX (PA9) як альтернативну функцію Push-Pull (вихід UART)
    // Налаштування безпосередньо для UART TX (PA9)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP; // Альтернативна функція Push-Pull
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // 3. Налаштовуємо ніжку RX (PA10) як плаваючий вхід (вхід UART)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING; // Вхід без підтяжки
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // 4. Конфігурація самого USART1
    USART_InitTypeDef USART_InitStructure;
    
    USART_InitStructure.USART_BaudRate = 115200; // Швидкість передачі
    USART_InitStructure.USART_WordLength = USART_WordLength_8b; // 8 біт даних
    USART_InitStructure.USART_StopBits = USART_StopBits_1; // 1 стоп-біт
    USART_InitStructure.USART_Parity = USART_Parity_No; // Без контролю парності
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None; // Без керування потоком
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx; // Увімкнути і прийом, і передачу

    USART_Init(USART1, &USART_InitStructure);

    // 5. Вмикаємо USART1
    USART_Cmd(USART1, ENABLE);
}
//******************************************************************
// Прості допоміжні функції для відправки даних
void UART1_SendByte(uint8_t byte) {
    // Чекаємо, поки очиститься регістр передачі (TXE - Transmit Data Register Empty)
    while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
    USART_SendData(USART1, byte);
}
//******************************************************************
void UART1_SendString(char* str) {
    while (*str) {
        UART1_SendByte(*str++);
    }
}
//******************************************************************
// Допоміжна функція: чекає та приймає 1 байт із таймаутом
uint8_t UART1_ReadByte(void) {
    while (USART_GetFlagStatus(USART1, USART_FLAG_RXNE) == RESET);
    return USART_ReceiveData(USART1);
}
//******************************************************************
// Допоміжна функція прийому байта з таймаутом (програмним)
// Повертає -1, якщо таймаут вийшов
int32_t UART1_ReadByteTimeout(uint32_t timeout_val) {
    while (USART_GetFlagStatus(USART1, USART_FLAG_RXNE) == RESET) {
        // ?? АНТИ-ЗАВИСАННЯ: Перевіряємо та скидаємо помилку переповнення (Overrun Error)
        if (USART_GetFlagStatus(USART1, USART_FLAG_ORE) == SET) {
            // Щоб скинути прапорець ORE у STM32F1, потрібно спочатку 
            // зчитати регістр SR (що ми вже зробили вище), а потім регістр DR.
            (void)USART_ReceiveData(USART1); 
        }

        if (timeout_val > 0) {
            timeout_val--;
        } else {
            return -1; // Таймаут
        }
    }
    return USART_ReceiveData(USART1);
}
//******************************************************************


