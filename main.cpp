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
   // Створюємо мапу структури поверх фізичної адреси у Flash
    const FirmwareHeader_t* header = (const FirmwareHeader_t*)SLOT0_START_ADDRESS;

    // 1. ПЕРЕВІРКА МАГІЧНОГО ЧИСЛА (Повинно бути "STM3" -> 0x334D5453)
    if (header->magic_number != 0x334D5453) {
        UART1_SendString("Validation Failed: Invalid Magic Number! App missing?\r\n");
        return; 
    }

    // 2. ПЕРЕВІРКА ПРИВ'ЯЗКИ ДО ЗАЛІЗА (Hardware ID)
    if (header->hardware_id[0] != 'S' || header->hardware_id[1] != 'T') {
        UART1_SendString("Validation Failed: Firmware is not for this Hardware ID!\r\n");
        return;
    }

    // 3. ?? АПАРАТНА ПЕРЕВІРКА РОЗШИФРОВАНОГО КОДУ (clean_crc32)
    // Тіло програми починається строго після 64-байтового хедера
    const uint32_t* app_body_start = (const uint32_t*)(SLOT0_START_ADDRESS + 64);
    
    // Розраховуємо кількість 32-бітних слів у чистому тілі програми
    uint32_t app_body_bytes = header->file_size - 64;
    uint32_t app_body_words = (app_body_bytes + 3) / 4; 

    // Запускаємо апаратний блок CRC32 для розшифрованого Слоту 0
    uint32_t calculated_clean_crc = Calculate_Hardware_CRC32(app_body_start, app_body_words);

    // Звіряємо порахований CRC32 із чистим CRC32, який зберіг Python у полі clean_crc32
    if (calculated_clean_crc != header->clean_crc32) {
        UART1_SendString("Validation Failed: Clean CRC32 Mismatch! Code in Slot 0 is corrupted.\r\n");
        return; // Блокуємо стрибок, прошивка бита або дешифратор дав збій
    }

    // ЯКЩО ВСЕ ОК — СТРИБАЄМО!
    UART1_SendString("Validation Success! Active Bank (Slot 0) Clean CRC32 is valid.\r\n");
    UART1_SendString("Jumping to Application...\r\n");
    
    // Вимикаємо апаратний CRC перед виходом
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_CRC, DISABLE);
    
    // Наш стандартний і перевірений джамп на адресу вектора додатка (0x08002840)
		DeinitAll();
    USART_Cmd(USART1, DISABLE);
    RCC_DeInit();
    SysTick->CTRL = 0; 
    
    // Передаємо Stack Pointer та Reset Handler додатка
    __set_MSP(*(__IO uint32_t*) (SLOT0_START_ADDRESS + 64));
    
    pFunction Jump_To_App = (pFunction)(*(__IO uint32_t*)(SLOT0_START_ADDRESS + 64 + 4));
    Jump_To_App();
}
//*******************************************************************************
//точка входу в завантажувач
//*******************************************************************************
int main(void) {
    UART1_Init();
    TIM2_Init(); 

    // Вигрібаємо стартове сміття з UART
    while (USART_GetFlagStatus(USART1, USART_FLAG_RXNE) == SET || USART_GetFlagStatus(USART1, USART_FLAG_ORE) == SET) {
        (void)USART_ReceiveData(USART1);
    }

    UART1_SendString("\r\n=== PROFESSIONAL DUAL-BANK BOOTLOADER ===\r\n");

    // ?? КРОК 1: Перевіряємо, чи лежить у СЛОТІ 1 свіже оновлення
    const FirmwareHeader_t* slot1_header = (const FirmwareHeader_t*)SLOT1_START_ADDRESS;
    
    if (slot1_header->magic_number == 0x334D5453) { // Знайшли "STM3"
        UART1_SendString("New firmware found in Slot 1. Validating CRC32...\r\n");
        
        const uint32_t* body_start = (const uint32_t*)(SLOT1_START_ADDRESS + 64);
        uint32_t body_words = (slot1_header->file_size - 64 + 3) / 4;
        
        // Перевіряємо апаратним CRC32, чи цілий файл у Слоті 1
        if (Calculate_Hardware_CRC32(body_start, body_words) == slot1_header->firmware_crc32) {
            // Файл ідеальний! Запускаємо копіювання
            Bootloader_UpgradeFirmware_Decrypt(slot1_header->file_size);
            
            for(volatile int d = 0; d < 1000000; d++); // Пауза для UART
            NVIC_SystemReset(); // Ресет, щоб чистий процесор запустив Слот 0
        } else {
            UART1_SendString("Slot 1 CRC mismatch! Erasing broken firmware...\r\n");
            Bootloader_EraseSlot1();
        }
    }

    // ?? КРОК 2: Звичайний запуск. Чекаємо 3 секунди на примусове оновлення 'U'
    UART1_SendString("Press '1' within 3 seconds for manual update...\r\n");
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
        UART1_SendString("Entering Update Mode. Send file via Tera Term XMODEM...\r\n");
        
        // ?? Обов'язково поверни у кінець Xmodem_Receive() ресет: NVIC_SystemReset();
        Xmodem_Receive(); 
    } else {
        // ?? КРОК 3: Користувач мовчить, оновлень немає — перевіряємо і запускаємо робочий Слот 0
        UART1_SendString("Validating Active Bank (Slot 0)...\r\n");
        Validate_And_Launch_Slot0();
        
        // Якщо Слот 0 битий — застрягаємо в аварійному режимі відновлення
        UART1_SendString("Emergency Mode: Slot 0 is corrupted! Waiting for XMODEM recovery...\r\n");
        Xmodem_Receive(); 
    }
}

//*******************************************************************************
