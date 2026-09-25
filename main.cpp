#include "stm32f10x.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_usart.h"            // Keil::Device:StdPeriph Drivers:USART
#include "global.h"
#include "my_UART.h"
#include "my_flash.h"
#include "my_XMODEM.h"
#include "my_tim2_delay.h"



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
    TIM2_Init(); // ?? Вмикаємо наш точний таймер
    
    // Очищаємо сміття з UART перед стартом
    while (USART_GetFlagStatus(USART1, USART_FLAG_RXNE) == SET || USART_GetFlagStatus(USART1, USART_FLAG_ORE) == SET) {
        (void)USART_ReceiveData(USART1);
    }

    UART1_SendString((char*)"Bootloader Active. Press '1' to update (3 sec timeout)...\r\n");

    uint8_t update_mode = 0;
    
    // Засікаємо початковий час у мілісекундах (3 секунди = 3000 циклів по 1 мс)
    uint32_t ms_passed = 0;

    while (ms_passed < 3000) {
        // Перевіряємо, чи прийшла команда 'U'
        if (USART_GetFlagStatus(USART1, USART_FLAG_RXNE) == SET) {
            if (USART_ReceiveData(USART1) == '1') {
                update_mode = 1;
                break;
            }
        }
        
        // Робимо точну паузу в 1 мілісекунду і збільшуємо лічильник часу
        delay_ms(1);
        ms_passed++;
    }

    if (update_mode) {
        Xmodem_Receive();
    } else {
        UART1_SendString((char*)"Timeout reached. Starting App...\r\n");
        
        // Перед стрибком ОБОВ'ЯЗКОВО вимикаємо і таймер теж!
        TIM_Cmd(TIM2, DISABLE); 
        USART_Cmd(USART1, DISABLE);
			  RCC_DeInit();
				GPIO_DeInit(GPIOA);
			  SysTick->CTRL = 0; 
			
        Jump_To_Application();
    }
}
