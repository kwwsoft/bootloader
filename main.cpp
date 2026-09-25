#include "stm32f10x.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_usart.h"            // Keil::Device:StdPeriph Drivers:USART
#include "global.h"
#include "my_UART.h"
#include "my_flash.h"
#include "my_XMODEM.h"
#include "my_tim2_delay.h"




//*******************************************************************************
typedef void (*pFunction)(void);
//*******************************************************************************
void DeinitAll(){
        TIM_Cmd(TIM2, DISABLE); 
        USART_Cmd(USART1, DISABLE);
				GPIO_DeInit(GPIOA);
}
//*******************************************************************************
void Jump_To_Application(void) {
    // Перевіряємо валідність Stack Pointer.
    // Для CBT6 верхня межа RAM така ж (20 КБ), тому маска 0x2FFE0000 працює коректно.
    if (((*(__IO uint32_t*)APPLICATION_ADDRESS) & 0x2FFE0000) == 0x20000000) {
        
        // Отримуємо адресу Reset_Handler основної програми
        uint32_t JumpAddress = *(__IO uint32_t*) (APPLICATION_ADDRESS + 4);
        pFunction Jump_To_App = (pFunction) JumpAddress;
        
        // Деініціалізація периферії перед стрибком
				DeinitAll();
        RCC_DeInit();
			  SysTick->CTRL = 0; 
	       
        // Встановлюємо покажчик стеку основної програми
        __set_MSP(*(__IO uint32_t*) APPLICATION_ADDRESS);
        
        // Стрибаємо в основну програму
        Jump_To_App();
    }
}
//*******************************************************************************
void Validate_And_Launch_Slot0(void) {
    // Створюємо мапу структури прямо поверх фізичної адреси у Flash
    const FirmwareHeader_t* header = (const FirmwareHeader_t*)SLOT0_START_ADDRESS;

    // 1. ПЕРЕВІРКА МАГІЧНОГО ЧИСЛА
    if (header->magic_number != 0x334D5453) {
        UART1_SendString("Validation Failed: Invalid Magic Number! App missing?\r\n");
        return; // Стрибати не можна, залишаємося в бутлоадері
    }

    // 2. ПЕРЕВІРКА ПРИВ'ЯЗКИ ДО ЗАЛІЗА
    // Перевіримо перші кілька символів Hardware ID, щоб переконатися, що софт наш
    if (header->hardware_id[0] != 'S' || header->hardware_id[1] != 'T') {
        UART1_SendString("Validation Failed: Firmware is not for this Hardware ID!\r\n");
        return;
    }

    // 3. АПАРАТНА ПЕРЕВІРКА ЦІЛІСНОСТІ (CRC32)
    // Тіло програми починається одразу за 64-байтовим хедером
    const uint32_t* app_body_start = (const uint32_t*)(SLOT0_START_ADDRESS + 64);
    
    // Рахуємо скільки 32-бітних слів займає тіло програми (загальний розмір мінус 64 байти хедера)
    uint32_t app_body_bytes = header->file_size - 64;
    uint32_t app_body_words = (app_body_bytes + 3) / 4; // Округлення вгору до цілого слова

    // Запускаємо апаратний розрахунок
    uint32_t calculated_crc = Calculate_Hardware_CRC32(app_body_start, app_body_words);

    // Звіряємо паспортний CRC із фактичним
    if (calculated_crc != header->firmware_crc32) {
        UART1_SendString("Validation Failed: CRC32 Mismatch! Firmware is corrupted.\r\n");
        // Можна вивести в консоль для дебагу, що чекали і що отримали:
        // (calculated_crc VS header->firmware_crc32)
        return; 
    }

    // ЯКЩО ВСЕОК — РОБИМО СТРИБОК!
    UART1_SendString("Validation Success! Application CRC32 is valid.\r\n");
    UART1_SendString("Jumping to Application...\r\n");
    
    // Вимикаємо апаратний CRC перед виходом, повертаємо залізо у чистий стан
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_CRC, DISABLE);
    
    // Наш перевірений джамп на адресу вектора додатка (0x08002800 + 64 = 0x08002840)
    USART_Cmd(USART1, DISABLE);
    RCC_DeInit();
    SysTick->CTRL = 0; 
    
    // Передаємо Stack Pointer та Reset Handler додатка
    __set_MSP(*(__IO uint32_t*) (SLOT0_START_ADDRESS + 64));
    
    // Беремо адресу Reset Handler (вона лежить на 4 байти далі таблиці векторів програми)
    pFunction Jump_To_App = (pFunction)(*(__IO uint32_t*)(SLOT0_START_ADDRESS + 64 + 4));
    Jump_To_App();
}
//*******************************************************************************
//*******************************************************************************
//точка входу в лоадер
/*int main(void) {
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
 		
        Jump_To_Application();
    }
}*/
int main(void) {
    UART1_Init();
    TIM2_Init(); 
    
    // Очищаємо UART від стартового бруду
    while (USART_GetFlagStatus(USART1, USART_FLAG_RXNE) == SET || USART_GetFlagStatus(USART1, USART_FLAG_ORE) == SET) {
        (void)USART_ReceiveData(USART1);
    }

    UART1_SendString("\r\n=== DUAL-BANK BOOTLOADER V1.0 ===\r\n");
    UART1_SendString("Press '1' within 3 seconds to force update mode...\r\n");

    uint8_t force_update = 0;
    uint32_t ms_passed = 0;

    while (ms_passed < 3000) {
        if (USART_GetFlagStatus(USART1, USART_FLAG_RXNE) == SET) {
            if (USART_ReceiveData(USART1) == '1') {
                force_update = 1;
                break;
            }
        }
        delay_ms(1);
        ms_passed++;
    }

    if (force_update) {
        UART1_SendString("Force Update Mode requested by user.\r\n");
        // Сюди ми згодом підключимо прийом XMODEM, але вже в СЛОТ 1!
        while(1); 
    } else {
        UART1_SendString("No user request. Validating Active Bank (Slot 0)...\r\n");
        
        // Перевіряємо паспорт програми. Якщо вона ціла — завантажувач сам передасть керування
        Validate_And_Launch_Slot0();
        
        // Якщо ми опинилися тут — значить додаток у Слоті 0 пошкоджений (не пройшов CRC)
        UART1_SendString("System halted. Entering Emergency Recovery Mode. Please re-flash via XMODEM...\r\n");
        // Сюди теж підключимо XMODEM
        while(1);
    }
}
//*******************************************************************************


