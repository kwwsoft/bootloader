#ifndef __my_GLOBAL_H
#define __my_GLOBAL_H


// Адреса старту основної програми (10 КБ від початку Flash)
#define APPLICATION_ADDRESS     0x08002800
// 1 КБ для нашого чипа CBT6
#define FLASH_PAGE_SIZE     		1024   
// Кінець пам'яті 128 КБ для CBT6
#define MCU_FLASH_END       		0x08020000  
//розмір пакета прийому прошивки по уарт за 1 раз
#define PACKET_SIZE  						133


// Керуючі символи XMODEM
#define SOH  0x01
#define EOT  0x04
#define ACK  0x06
#define NAK  0x15
#define CAN  0x18  // Cancel (скасування)
#define CTRLC 0x03




#endif
