#include "stm32f10x.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_usart.h"            // Keil::Device:StdPeriph Drivers:USART
#include "global.h"
#include "my_UART.h"
#include "my_flash.h"



typedef void (*pFunction)(void);

uint8_t rx_buffer[PACKET_SIZE]; // Буфер в RAM для прийому одного пакета


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
    UART1_SendString((char*)"Bootloader Ready. Press 'U' to update...\r\n");

    uint32_t timeout = 6000000; 
    uint8_t update_mode = 0;

    while (timeout--) {
        if (USART_GetFlagStatus(USART1, USART_FLAG_RXNE) == SET) {
            if (USART_ReceiveData(USART1) == 'U') {
                update_mode = 1;
                break;
            }
        }
    }

    if (update_mode) {
        UART1_SendString((char*)"Entering Update Mode. Send 4 bytes of file size...\r\n");

        // 1. Приймаємо 4 байти розміру файлу прошивки
        uint32_t firmware_size = 0;
        firmware_size |= ((uint32_t)UART1_ReadByte() << 0);
        firmware_size |= ((uint32_t)UART1_ReadByte() << 8);
        firmware_size |= ((uint32_t)UART1_ReadByte() << 16);
        firmware_size |= ((uint32_t)UART1_ReadByte() << 24);

        // Перевіряємо, чи файл взагалі поміститься в нашу пам'ять (макс ~118 КБ)
        if (firmware_size > (MCU_FLASH_END - APPLICATION_ADDRESS)) {
            UART1_SendString((char*)"Error: File too big!\r\n");
            NVIC_SystemReset(); // Перезавантаження
        }

        UART1_SendString((char*)"Erasing Flash... Please wait.\r\n");
        // 2. Стираємо Flash-пам'ять під нову прошивку
        Bootloader_EraseAppSpace();

        // Повідомляємо ПК, що ми готові приймати пакети коду
        UART1_SendByte('K'); 

        // 3. Цикл прийому пакетів даних
        uint32_t bytes_written = 0;
        
        while (bytes_written < firmware_size) {
            // Визначаємо розмір поточного пакета (останній пакет може бути меншим за 256 байт)
            uint32_t current_packet_size = PACKET_SIZE;
            if ((firmware_size - bytes_written) < PACKET_SIZE) {
                current_packet_size = firmware_size - bytes_written;
            }

            // Наповнюємо буфер байтами з UART
            for (uint32_t p = 0; p < current_packet_size; p++) {
                rx_buffer[p] = UART1_ReadByte();
            }

            // Якщо розмір пакета непарний, вирівнюємо його для коректного запису по 16 біт
            if (current_packet_size % 2 != 0) {
                rx_buffer[current_packet_size] = 0xFF; // Дописуємо пустий байт
                current_packet_size++;
            }

            // Записуємо прийнятий пакет безпосередньо у Flash-пам'ять
            Bootloader_WriteFlash(APPLICATION_ADDRESS + bytes_written, rx_buffer, current_packet_size);
            
            bytes_written += current_packet_size;

            // Надсилаємо ПК підтвердження: "Пакет записано, давай наступний"
            UART1_SendByte('A'); 
        }

        UART1_SendString((char*)"\r\nUpdate Successful! Restarting...\r\n");
        for(volatile int d=0; d<1000000; d++); // Маленька пауза, щоб UART встиг виплюнути текст
        
        // Перезавантажуємо контролер. Після рестарту він вийде в таймаут і запустить нову прошивку!
        NVIC_SystemReset(); 

    } else {
        UART1_SendString((char*)"Timeout. Jumping to App...\r\n");
        USART_Cmd(USART1, DISABLE); 
        Jump_To_Application();
    }
}