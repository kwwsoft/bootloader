#ifndef __my_TIM2DELAY_H
#define __my_TIM2DELAY_H

void TIM2_Init(void);

// Точна затримка в мікросекундах (максимум 65535 мкс за один виклик)
void delay_us(uint16_t us);
// Точна затримка в мілісекундах
void delay_ms(uint32_t ms);





#endif
