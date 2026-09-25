#ifndef __my_FLASH_H
#define __my_FLASH_H


// Функція очищення Flash-пам'яті під основну програму
void Bootloader_EraseAppSpace(void);

// Функція очищення Flash-пам'яті під тимчасову заливку програми
void Bootloader_EraseSlot1(void);

// Функція запису масиву байтів у Flash (пишемо по 2 байти)
void Bootloader_WriteFlash(uint32_t start_address, uint8_t* data_buffer, uint32_t data_length);

//апаратна ЦРЦ32 
uint32_t Calculate_Hardware_CRC32(const uint32_t* start_address, uint32_t words_count);

//перенесення прошивки із слот1 в робочий слот0
void Bootloader_UpgradeFirmware(uint32_t firmware_size);




#endif
