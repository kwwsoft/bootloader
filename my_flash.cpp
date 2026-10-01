
#include "stm32f10x.h"                  // Device header
#include "misc.h"                       // Keil::Device:StdPeriph Drivers:Framework
#include "stm32f10x_flash.h"            // Keil::Device:StdPeriph Drivers:Flash
#include "stm32f10x_crc.h"              // Keil::Device:StdPeriph Drivers:CRC

// ?? Кажемо компілятору C++, що tiny-AES — це чистий Сі код
extern "C" {
    #include "aes.h"
}
#include "global.h"
#include "my_UART.h"
#include "my_flash.h"

//*******************************************************************************
//ключ для шифрування в лоадері
//вектор буде в додатку кожен раз різний
static const uint8_t BOOTLOADER_AES_KEY[16] = {
    0x55, 0xCC, 0x1A, 0x83, 0xF2, 0x6E, 0xBB, 0x04, 
	  0x9D, 0x41, 0x7A, 0xCE, 0x8B, 0x3F, 0x62, 0x94
};
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
//з шифруванням прошивки
/*
void Bootloader_UpgradeFirmware_Decrypt(uint32_t firmware_size) {
	
    // Хедер нової прошивки лежить на початку Слоту 1
    const FirmwareHeader_t* slot1_header = (const FirmwareHeader_t*)SLOT1_START_ADDRESS;
    
    uint32_t encrypted_body_bytes = firmware_size - 64;
    
    // Створюємо буфер для розшифрування одного блоку (16 байт) у RAM
    // Вирівнюємо на 4 байти для безпеки запису Flash
    uint8_t __attribute__((aligned(4))) crypto_block[16];
    
    const uint8_t* src_bytes = (const uint8_t*)(SLOT1_START_ADDRESS + 64);
    uint32_t dst_address = SLOT0_START_ADDRESS + 64; // Пишемо тіло програми після її хедера

    struct AES_ctx ctx;
    
    // Ініціалізуємо контекст AES-128-CBC ключем звідси та IV, якй ми взяли прямо з хедера!
    AES_init_ctx_iv(&ctx, BOOTLOADER_AES_KEY, slot1_header->aes_iv);

    UART1_SendString("Upgrading: Decrypting Slot 1 into Slot 0...\r\n");

    // 1. Спочатку копіюємо сам ХЕДЕР додатка зі Слоту 1 у Слот 0 недоторканим
    // (Але занулюємо там поля ключа та IV для безпеки, щоб у Слоті 0 їх не було видно!)
    uint8_t __attribute__((aligned(4))) clean_header_buf[64];
    for(int i=0; i<64; i++) {
        clean_header_buf[i] = ((uint8_t*)SLOT1_START_ADDRESS)[i];
    }
    // Затираємо ключі в копії хедера для Слоту 0 (опціонально для супер-захисту)
    for(int i=32; i<64; i++) { clean_header_buf[i] = 0xFF; }
    
    // Стираємо Слот 0
    // 1. СТИРАЄМО СЛОТ 0 (Суворо ДО межі Слоту 1!)
    uint32_t current_address = SLOT0_START_ADDRESS;
    FLASH_Unlock();
    FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);
    
    while (current_address < SLOT1_START_ADDRESS) {
        FLASH_ErasePage(current_address);
        current_address += FLASH_PAGE_SIZE;
    }

    //Bootloader_EraseSlot0_Only(); 
    
    // Записуємо очищений хедер у Слот 0
    Bootloader_WriteFlash(SLOT0_START_ADDRESS, clean_header_buf, 64);

    // 2. ГОЛОВНИЙ ЦИКЛ ДЕШИФРУВАННЯ: Ідемо кроком по 16 байт (розмір блоку AES)
    for (uint32_t offset = 0; offset < encrypted_body_bytes; offset += 16) {
        
        // Копіюємо 16 зашифрованих байт зі Flash (Слот 1) в оперативку (RAM)
        for (int b = 0; b < 16; b++) {
            crypto_block[b] = src_bytes[offset + b];
        }

        // ?? МАГІЯ: Розшифровуємо цей 16-байтний блок прямо в оперативці!
        AES_CBC_decrypt_buffer(&ctx, crypto_block, 16);

        // Записуємо вже розшифровані 16 байт (чистий код) у робочий Слот 0!
        Bootloader_WriteFlash(dst_address + offset, crypto_block, 16);
    }

    // 3. Очищаємо за собою Слот 1
    Bootloader_EraseSlot1();
    
    UART1_SendString("Decryption complete! System rebooting...\r\n");
}
*/
#include <string.h> // Для memset
#include "stm32f10x_flash.h"
#include "global.h"
#include "my_flash.h"
#include "my_UART.h"

extern "C" {
    #include "aes.h"
}

// =========================================================================
// КЛЮЧ-ПРИВИД: Зашитий намертво у Flash завантажувача. 
// Скрипт test.py на ПК використовує ТОЧНО такий самий!
// =========================================================================
void Bootloader_UpgradeFirmware_Decrypt(uint32_t firmware_size) {
    // Початок хедера у зашифрованому Слоті 1
    const FirmwareHeader_t* slot1_header = (const FirmwareHeader_t*)SLOT1_START_ADDRESS;
    
    // Розмір зашифрованого тіла програми (загальний розмір мінус 64 байти хедера)
    uint32_t encrypted_body_bytes = firmware_size - 64;
    
    // Тимчасові змінні адрес
    const uint8_t* src_bytes = (const uint8_t*)(SLOT1_START_ADDRESS + 64);
    uint32_t dst_address = SLOT0_START_ADDRESS + 64; // Пишемо код строго після хедера

    // Створюємо контекст AES і вирівняний 16-байтний RAM-буфер для поблочної роботи
    struct AES_ctx ctx;
    uint8_t __attribute__((aligned(4))) crypto_block[16];
    
    // Ініціалізуємо AES: ключ беремо з Flash бутлоадера, а IV — динамічний із хедера Слоту 1
    AES_init_ctx_iv(&ctx, BOOTLOADER_AES_KEY, slot1_header->aes_iv);

    UART1_SendString("Upgrading: Decrypting Slot 1 into Slot 0...\r\n");

    // 1. Формуємо безпечний чистий хедер для робочого Слоту 0
    uint8_t __attribute__((aligned(4))) clean_header_buf[64];
    for (int i = 0; i < 64; i++) {
        clean_header_buf[i] = ((uint8_t*)SLOT1_START_ADDRESS)[i];
    }
    
    // Параноя №1: Повністю вичищаємо поля ключів та IV в копії хедера перед записом у Слот 0.
    // Додаток у Слоті 0 взагалі не повинен знати, що прошивка шифрувалася.
    for (int i = 32; i < 64; i++) { 
        clean_header_buf[i] = 0xFF; 
    }
    
    // 2. Стираємо робочий Слот 0 (строго до межі Слоту 1)
    uint32_t current_address = SLOT0_START_ADDRESS;
    FLASH_Unlock();
    FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);
    
    while (current_address < SLOT1_START_ADDRESS) {
        FLASH_ErasePage(current_address);
        current_address += FLASH_PAGE_SIZE;
    }
    
    // Записуємо підготовлений безпечний хедер у Слот 0
    Bootloader_WriteFlash(SLOT0_START_ADDRESS, clean_header_buf, 64);

    // 3. Головний цикл дешифрування: йдемо кроком по 16 байт
    for (uint32_t offset = 0; offset < encrypted_body_bytes; offset += 16) {
        
        // Читаємо 16 зашифрованих байт із Flash Слоту 1 в RAM-буфер
        for (int b = 0; b < 16; b++) {
            crypto_block[b] = src_bytes[offset + b];
        }

        // Розшифровуємо блок прямо в оперативці
        AES_CBC_decrypt_buffer(&ctx, crypto_block, 16);

        // Записуємо розшифрований чистий код у Слот 0
        Bootloader_WriteFlash(dst_address + offset, crypto_block, 16);
    }

    // =========================================================================
    // ?? ПАРАНОЇДАЛЬНЕ ОЧИЩЕННЯ RAM (Військовий стандарт безпеки)
    // =========================================================================
    
    // Випалюємо контекст AES (там лежать розгорнуті в RAM раундові ключі KeyExpansion!)
    memset(&ctx, 0x00, sizeof(ctx));
    
    // Вичищаємо проміжний буфер даних, де міг залишитися останній шматок коду
    memset(crypto_block, 0x00, sizeof(crypto_block));
    
    // Вичищаємо буфер хедера
    memset(clean_header_buf, 0x00, sizeof(clean_header_buf));
    
    // 4. Очищаємо за собою Слот 1, щоб видалити зашифрований бінарник
    Bootloader_EraseSlot1();
    
    UART1_SendString("Decryption complete! RAM Secured. System rebooting...\r\n");
		
		  // Апаратний ресет повністю випалить регітри процесора на залізному рівні!
    NVIC_SystemReset();
}


//*******************************************************************************
