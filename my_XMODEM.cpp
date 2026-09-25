#include "stm32f10x.h"                  // Device header
#include "misc.h"                       // Keil::Device:StdPeriph Drivers:Framework
#include "stm32f10x_usart.h"            // Keil::Device:StdPeriph Drivers:USART
#include "global.h"
#include "my_flash.h"
#include "my_UART.h"
//***********************************************************************
uint8_t packet_buf[PACKET_SIZE]; // Буфер в RAM для прийому одного пакета
// Додаємо директиву компілятора для залізного вирівнювання масиву в RAM
uint8_t clean_data_buf[128] __attribute__((aligned(4)));
//***********************************************************************

// Перевірений підрахунок CRC16 CCITT
uint16_t crc16_ccitt(const uint8_t *buf, int len) {
    uint16_t crc = 0;
    while (len--) {
        crc ^= (*buf++ << 8);
        for (int i = 0; i < 8; ++i) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc = crc << 1;
            }
        }
    }
    return crc;
}

//***********************************************************************
void Xmodem_Receive(void) {
    uint8_t expected_packet_num = 1;
    uint32_t flash_write_address = SLOT0_START_ADDRESS;
    uint8_t file_erased = 0;
    
    int32_t first_byte = 0;
    int32_t rx_byte = 0;
    uint16_t crc_calc = 0;
    uint16_t crc_received = 0;
    uint8_t start_success = 0;

    // Спроба встановити зв'язок з Tera Term
    for (int retry = 0; retry < 15; retry++) {
        UART1_SendByte('C');
        first_byte = UART1_ReadByteTimeout(4000000);
        
        if (first_byte == SOH || first_byte == EOT) {
            packet_buf[0] = (uint8_t)first_byte; 
            start_success = 1;
            break; 
        }
    }
    
    if (!start_success) {
        UART1_SendByte(CAN);
        NVIC_SystemReset();
    }

    while (1) {
        if (flash_write_address != SLOT0_START_ADDRESS) {
            rx_byte = UART1_ReadByteTimeout(4000000);
            if (rx_byte < 0) { 
                UART1_SendByte(NAK); 
                continue; 
            }
            packet_buf[0] = (uint8_t)rx_byte;
        }

        if (packet_buf[0] == EOT) {
            UART1_SendByte(ACK);
            break; 
        }
        
        if (packet_buf[0] == CAN || packet_buf[0] == CTRLC) {
            UART1_SendByte(ACK);
            NVIC_SystemReset();
        }

        if (packet_buf[0] != SOH) {
            UART1_SendByte(NAK);
            continue;
        }

        // Дочитуємо решту 132 байти пакета
        uint8_t packet_error = 0;
        for (int i = 1; i < 133; i++) {
            rx_byte = UART1_ReadByteTimeout(1000000);
            if (rx_byte < 0) {
                UART1_SendByte(NAK);
                packet_error = 1;
                break;
            }
            packet_buf[i] = (uint8_t)rx_byte;
        }

        if (packet_error) {
            continue;
        }

        uint8_t p_num = packet_buf[1]; 
        uint8_t p_inv = packet_buf[2]; 

        if (p_num != expected_packet_num) {
            if (p_num == (uint8_t)(expected_packet_num - 1)) {
                // Штучно вичищаємо буфер перед повторним ACK
                while (USART_GetFlagStatus(USART1, USART_FLAG_RXNE) == SET) { (void)USART_ReceiveData(USART1); }
                UART1_SendByte(ACK);
            } else {
                UART1_SendByte(NAK);
            }
            continue;
        }

        if ((uint8_t)(p_num + p_inv) != 255) {
            UART1_SendByte(NAK);
            continue;
        }

        crc_calc = crc16_ccitt(&packet_buf[3], 128);
        crc_received = (packet_buf[131] << 8) | packet_buf[132];

        if (crc_calc != crc_received) {
            UART1_SendByte(NAK); 
            continue;
        }

        for (int d = 0; d < 128; d++) {
            clean_data_buf[d] = packet_buf[3 + d];
        }

        if (!file_erased) {
            Bootloader_EraseAppSpace(); 
            file_erased = 1;
        }

        // Записуємо дані у Flash (STM32 засинає на час запису)
        Bootloader_WriteFlash(flash_write_address, clean_data_buf, 128);
        flash_write_address += 128;
        expected_packet_num++;
        
        // ?? КРИТИЧНЕ ОНОВЛЕННЯ: Жорстко чистимо апаратний буфер UART від "сміття",
        // яке могло туди прилетіти під час запису у Flash-пам'ять
        while (USART_GetFlagStatus(USART1, USART_FLAG_RXNE) == SET || USART_GetFlagStatus(USART1, USART_FLAG_ORE) == SET) {
            (void)USART_ReceiveData(USART1);
        }

        // Просимо наступний пакет
        UART1_SendByte(ACK);
    }

    for(volatile int d = 0; d < 1000000; d++);
    NVIC_SystemReset();
}
//***********************************************************************
