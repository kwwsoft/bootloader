
#include "stm32f10x.h"                  // Device header
#include "misc.h"                       // Keil::Device:StdPeriph Drivers:Framework
#include "stm32f10x_flash.h"            // Keil::Device:StdPeriph Drivers:Flash

#include "global.h"

#include "my_flash.h"




// Функція очищення Flash-пам'яті під основну програму
void Bootloader_EraseAppSpace(void) {
    uint32_t current_address = APPLICATION_ADDRESS;

    // Знімаємо захист з регістрів Flash
    FLASH_Unlock();
    
    // Очищаємо прапорці помилок (про всяк випадок)
    FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);

    // Посторінково стираємо пам'ять від початку програми і до кінця фізичного об'єму Flash
    while (current_address < MCU_FLASH_END) {
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
