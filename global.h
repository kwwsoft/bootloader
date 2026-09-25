#ifndef __my_GLOBAL_H
#define __my_GLOBAL_H


//*******************************************************************************
//тут починається хідер довжиною 65 байт в робочі області софта
#define SLOT0_START_ADDRESS  	0x08002800
// Адреса старту основної програми (10 КБ від початку Flash) + 64байт хідер
#define APPLICATION_ADDRESS     0x08002840
// 1 КБ для нашого чипа CBT6
#define FLASH_PAGE_SIZE     		1024   
// Кінець пам'яті 128 КБ для CBT6
#define MCU_FLASH_END       		0x08020000  
//розмір пакета прийому прошивки по уарт за 1 раз
#define PACKET_SIZE  						133

//*******************************************************************************
// Керуючі символи XMODEM
#define SOH  0x01
#define EOT  0x04
#define ACK  0x06
#define NAK  0x15
#define CAN  0x18  // Cancel (скасування)
#define CTRLC 0x03

//*******************************************************************************
#pragma pack(push, 1)
typedef struct {
    uint32_t magic_number;     // Повинно бути 0x334D5453 ("STM3")
    uint32_t file_size;        // Розмір у байтах, порахований Python
    uint32_t firmware_crc32;   // Контрольна сума, порахована Python
    
    uint8_t  ver_major;        
    uint8_t  ver_minor;        
    uint8_t  ver_patch;        
    uint8_t  reserved1;        
    uint32_t ver_build;        

    char     hardware_id[16];  // Назва плати
    uint8_t  reserved2[28];    // Наш вирівнюючий хвіст на 28 байтів
} FirmwareHeader_t;
#pragma pack(pop)
//*******************************************************************************



#endif
