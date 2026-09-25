
#include "stm32f10x.h"                  // Device header
#include "misc.h"                       // Keil::Device:StdPeriph Drivers:Framework
#include "stm32f10x_flash.h"            // Keil::Device:StdPeriph Drivers:Flash
#include "stm32f10x_crc.h"              // Keil::Device:StdPeriph Drivers:CRC

#include "global.h"
#include "my_UART.h"
#include "my_flash.h"



//*******************************************************************************
// Функція розрахунку апаратного CRC32 для ділянки Flash-пам'яті
// Вона приймає покажчик на початок даних та кількість 32-бітних слів
uint32_t Calculate_Hardware_CRC32(const uint32_t* start_address, uint32_t words_count) {
    // 1. Вмикаємо тактування модуля апаратного CRC (він висить на шині AHB)
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_CRC, ENABLE);
    
    // 2. Скидаємо модуль (внутрішній регістр встановлюється у 0xFFFFFFFF)
    CRC_ResetDR();
    
    // 3. Послідовно згодовуємо 32-бітні слова в апаратний регістр даних
    for (uint32_t i = 0; i < words_count; i++) {
        // Записуємо слово в регістр, залізо самостійно за 4 такти оновить CRC
        CRC->DR = start_address[i];
    }
    
    // 4. Повертаємо фінальний результат розрахунку
    return CRC_GetCRC();
}
//*******************************************************************************
//Функція очищення Flash ТІЛЬКИ для Слоту 1
void Bootloader_EraseSlot1(void) {
    uint32_t current_address = SLOT1_START_ADDRESS;
    FLASH_Unlock();
    FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);

    // Стираємо пам'ять строго від початку Слоту 1 і до кінця Flash
    while (current_address < MCU_FLASH_END) {
        FLASH_ErasePage(current_address);
        current_address += FLASH_PAGE_SIZE;
    }
    FLASH_Lock();
}
//*******************************************************************************

// Функція очищення Flash-пам'яті під основну програму
void Bootloader_EraseAppSpace(void) {
    uint32_t current_address = APPLICATION_ADDRESS;

    // Знімаємо захист з регістрів Flash
    FLASH_Unlock();
    
    // Очищаємо прапорці помилок (про всяк випадок)
    FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);

    // Посторінково стираємо пам'ять від початку програми і до кінця фізичного об'єму Flash
    while (current_address < SLOT1_START_ADDRESS) {
        // FLASH_ErasePage повертає статус. У реальному коді варто перевіряти, чи він FLASH_COMPLETE
        FLASH_ErasePage(current_address);
        current_address += FLASH_PAGE_SIZE; // Переходимо до наступного кілобайту
    }

    // Блокуємо доступ назад
    FLASH_Lock();
}
//*******************************************************************************

// Функція запису масиву байтів у Flash (пишемо по 2 байти)
void Bootloader_WriteFlash(uint32_t start_address, uint8_t* data_buffer, uint32_t data_length) {
    uint32_t i;
    
    FLASH_Unlock();
    FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);

    // Ітеруємося по масиву з кроком 2 байти
    for (i = 0; i < data_length; i += 2) {
        // Збираємо два 8-бітних байти в одне 16-бітне півслово (Little Endian)
        uint16_t half_word = data_buffer[i] | (data_buffer[i + 1] << 8);
        
        // Записуємо у Flash
        FLASH_ProgramHalfWord(start_address + i, half_word);
    }

    FLASH_Lock();
}
//*******************************************************************************
void Bootloader_UpgradeFirmware(uint32_t firmware_size) {
    // Переводимо розмір файлу у 32-бітні слова (округлення вгору)
    uint32_t words_count = (firmware_size + 3) / 4; 
    
    const uint32_t* src = (const uint32_t*)SLOT1_START_ADDRESS;
    uint32_t* dst = (uint32_t*)SLOT0_START_ADDRESS;

    UART1_SendString("Upgrading firmware: Copying Slot 1 to Slot 0...\r\n");

    // 1. СТИРАЄМО СЛОТ 0 (Суворо ДО межі Слоту 1!)
    uint32_t current_address = SLOT0_START_ADDRESS;
    FLASH_Unlock();
    FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);
    
    while (current_address < SLOT1_START_ADDRESS) {
        FLASH_ErasePage(current_address);
        current_address += FLASH_PAGE_SIZE;
    }

    // 2. ПОКРОКОВО ПЕРЕНОСИМО ДАНІ
    for (uint32_t i = 0; i < words_count; i++) {
        uint32_t word = src[i];
        // Розбиваємо 32-бітне слово на два HalfWord для STM32F1
        uint16_t low_hw = (uint16_t)(word & 0xFFFF);
        uint16_t high_hw = (uint16_t)((word >> 16) & 0xFFFF);

        FLASH_ProgramHalfWord((uint32_t)&dst[i], low_hw);
        FLASH_ProgramHalfWord((uint32_t)&dst[i] + 2, high_hw);
    }

    // 3. ОЧИЩАЄМО СЛОТ 1 (Щоб не копіювати його по колу після наступного ресету)
    current_address = SLOT1_START_ADDRESS;
    while (current_address < MCU_FLASH_END) {
        FLASH_ErasePage(current_address);
        current_address += FLASH_PAGE_SIZE;
    }

    FLASH_Lock();
    UART1_SendString("Upgrade successful! Rebooting into new system...\r\n");
}
//*******************************************************************************
