#include "stm32f10x.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_tim.h"
#include "my_tim2_delay.h"
//****************************************************************

//****************************************************************
void TIM2_Init(void) {
    // 1. Вмикаємо тактування для модуля TIM2 (він висить на шині APB1)
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;

    // 2. Налаштовуємо дільник (Prescaler). 72 000 000 / (71 + 1) = 1 000 000 Гц (1 мкс)
    TIM_TimeBaseStructure.TIM_Prescaler = 71;
    
    // Лічильник рахує вгору (від 0 до Period)
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    
    // Період ставимо максимальний для 16 біт, щоб таймер не скидався занадто часто
    TIM_TimeBaseStructure.TIM_Period = 65535;
    
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;

    // Ініціалізуємо TIM2 з нашими налаштуваннями
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);

    // 3. Запускаємо таймер
    TIM_Cmd(TIM2, ENABLE);
}
//****************************************************************
// Точна затримка в мікросекундах (максимум 65535 мкс за один виклик)
void delay_us(uint16_t us) {
    // Обнуляємо лічильник таймера
    TIM_SetCounter(TIM2, 0);
    
    // Чекаємо, поки лічильник дорахує до переданого значення
    while (TIM_GetCounter(TIM2) < us) {
        // Процесор просто чекає залізо
    }
}
//****************************************************************
// Точна затримка в мілісекундах
void delay_ms(uint32_t ms) {
    while (ms--) {
        delay_us(1000); // 1 мілісекунда — це 1000 мікросекунд
    }
}
//****************************************************************

