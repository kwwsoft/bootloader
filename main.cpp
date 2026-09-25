#include "stm32f10x.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_usart.h"            // Keil::Device:StdPeriph Drivers:USART
#include "global.h"
#include "my_UART.h"
#include "my_flash.h"
#include "my_XMODEM.h"



typedef void (*pFunction)(void);


void Jump_To_Application(void) {
    // Перевіряємо валідність Stack Pointer.
    // Для CBT6 верхня межа RAM така ж (20 КБ), тому маска 0x2FFE0000 працює коректно.
    if (((*(__IO uint32_t*)APPLICATION_ADDRESS) & 0x2FFE0000) == 0x20000000) {
        
        // Отримуємо адресу Reset_Handler основної програми
        uint32_t JumpAddress = *(__IO uint32_t*) (APPLICATION_ADDRESS + 4);
        pFunction Jump_To_App = (pFunction) JumpAddress;
        
        // Деініціалізація периферії перед стрибком
        RCC_DeInit();
        SysTick->CTRL = 0; 
        
        // Встановлюємо покажчик стеку основної програми
        __set_MSP(*(__IO uint32_t*) APPLICATION_ADDRESS);
        
        // Стрибаємо в основну програму
        Jump_To_App();
    }
}

int main(void) {
    UART1_Init();
    UART1_SendString((char*)"Bootloader Active. Press '1' to update...\r\n");

    uint32_t timeout = 6000000; 
    uint8_t update_mode = 0;

    while (timeout--) {
        if (USART_GetFlagStatus(USART1, USART_FLAG_RXNE) == SET) {
            if (USART_ReceiveData(USART1) == '1') {
                update_mode = 1;
                break;
            }
        }
    }

    if (update_mode) {
        Xmodem_Receive();
    } else {
        UART1_SendString((char*)"Timeout. Starting App...\r\n");

  			// Перед стрибком ОБОВ'ЯЗКОВО вимикаємо
        USART_Cmd(USART1, DISABLE);
			  RCC_DeInit();
				GPIO_DeInit(GPIOA);
			  SysTick->CTRL = 0;
				

			Jump_To_Application();
    }
}