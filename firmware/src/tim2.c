#include "stm32f4xx_conf.h"
#include "stm32f4xx.h"

#include "pp-printf.h"
#include "usart.h"
#include "common.h"

#include <stdint.h>

#define ETCH_STATE_START 0
#define ETCH_STATE_TRIGGER_LOW 1
#define ETCH_STATE_WAIT_CUTOFF 2


struct etch_state
{
    uint32_t start_at;
    int state;
    int enable_delay;
    int arc_width;
    int arc_release;
    int trigger_width;
    int clearance;
};

static volatile int irq_count = 0, lcnt = 0;

volatile struct etch_state etch;

/* Pin Assignent:
   PA0 = HV trigger
   PA1 = HV gate (cutoff)
   PB10 = HV charge
   PC12 = Touchdown
   */

void TIM2_IRQHandler()
{
    TIM_OCInitTypeDef  TIM_OCInitStructure;

    irq_count++;

    switch(etch.state)
    {
        case ETCH_STATE_START:
            TIM_ClearITPendingBit(TIM2, TIM_IT_CC1);

            TIM2->CCR1 = etch.start_at + etch.trigger_width;

            etch.state = ETCH_STATE_TRIGGER_LOW;

            break;

        case ETCH_STATE_TRIGGER_LOW:
            TIM_ClearITPendingBit(TIM2, TIM_IT_CC1);
            etch.state = ETCH_STATE_WAIT_CUTOFF;
            break;

        case ETCH_STATE_WAIT_CUTOFF:
            TIM_ClearITPendingBit(TIM2, TIM_IT_CC2);
            lcnt++;
            GPIO_ResetBits ( GPIOB, GPIO_Pin_10 ); // charge on

            break;

    }
}



void etch_sequence(uint32_t start_at)
{
    TIM_OCInitTypeDef  TIM_OCInitStructure;

    etch.trigger_width = 600;
    etch.arc_width = 40;
    etch.arc_release = 10000;
    etch.start_at = start_at;

    uint32_t ccmr1 = TIM2->CCMR1;
    ccmr1 &= (uint16_t)~TIM_CCMR1_OC1M;
    ccmr1 &= (uint16_t)~TIM_CCMR1_CC1S;
    ccmr1 |= TIM_OCMode_Toggle;
    uint32_t ccer = TIM2->CCER;

    ccer &= (uint16_t)~TIM_CCER_CC1P;
    ccer |= TIM_OCPolarity_Low;
    ccer |= TIM_OutputState_Enable;

    etch.state = ETCH_STATE_START;

    TIM2->CCMR1 = ccmr1;
    TIM2->CCER = ccer;
    TIM2->CCR1 = start_at;

    ccmr1 = TIM2->CCMR1;
    ccmr1 &= (uint16_t)~TIM_CCMR1_OC2M;
    ccmr1 &= (uint16_t)~TIM_CCMR1_CC2S;
    ccmr1 |= TIM_ForcedAction_Active << 8;
    ccer = TIM2->CCER;

    TIM2->CCMR1 = ccmr1;

    ccmr1 &= (uint16_t)~TIM_CCMR1_OC2M;
    ccmr1 &= (uint16_t)~TIM_CCMR1_CC2S;
    ccmr1 |= TIM_OCMode_Toggle << 8;
    ccer = TIM2->CCER;

    ccer &= (uint16_t)~TIM_CCER_CC2P;
    ccer |= TIM_OCPolarity_Low << 4;
    ccer |= TIM_OutputState_Enable << 4;

    etch.state = ETCH_STATE_START;

    TIM2->CCMR1 = ccmr1;
    TIM2->CCER = ccer;
    TIM2->CCR2 = start_at + etch.trigger_width + etch.arc_width;

    GPIO_SetBits ( GPIOB, GPIO_Pin_10 ); // charge off


//    TIM_OC1Init(TIM2, &TIM_OCInitStructure);
    TIM_ITConfig(TIM2, TIM_IT_CC1, ENABLE);
    TIM_ITConfig(TIM2, TIM_IT_CC2, ENABLE);
    TIM_Cmd(TIM2, ENABLE);
}


void test_tim2()
{
    GPIO_InitTypeDef  GPIO_InitStructure;

    TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_DOWN ;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10 ;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_DOWN ;
    GPIO_Init(GPIOB, &GPIO_InitStructure);


    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
  GPIO_InitStructure.GPIO_Speed = GPIO_Fast_Speed;
  GPIO_Init(GPIOC, &GPIO_InitStructure);

    /* PA0 = TIM2.CH1 (HV trigger) */
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource0, GPIO_AF_TIM2);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource1, GPIO_AF_TIM2);

    GPIO_ResetBits ( GPIOB, GPIO_Pin_10 ); // charge on

    /* Time base configuration */
    TIM_TimeBaseStructure.TIM_Period = 0xffffffff;
    TIM_TimeBaseStructure.TIM_Prescaler = 0;
    TIM_TimeBaseStructure.TIM_ClockDivision = 0;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;

    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);
    TIM_OC1PreloadConfig(TIM2, TIM_OCPreload_Disable);
    TIM_ARRPreloadConfig(TIM2, DISABLE);

    NVIC_InitTypeDef   NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = TIM2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x01;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x01;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);


    uint32_t ccmr1 = TIM2->CCMR1;
    ccmr1 &= (uint16_t)~TIM_CCMR1_OC2M;
    ccmr1 &= (uint16_t)~TIM_CCMR1_CC2S;
    ccmr1 |= TIM_ForcedAction_InActive << 8;
    uint32_t ccer = TIM2->CCER;

    ccer &= (uint16_t)~TIM_CCER_CC2P;
    ccer |= TIM_OCPolarity_Low << 4;
    ccer |= TIM_OutputState_Enable << 4; /* TIM2 CH2 = inactive by default (cutoff) */

    etch.state = ETCH_STATE_START;

    TIM2->CCMR1 = ccmr1;
    TIM2->CCER = ccer;


    ccer &= (uint16_t)~TIM_CCER_CC1P;
    ccer |= TIM_OCPolarity_Low;
    ccer |= TIM_OutputState_Enable;

    ccmr1 = TIM2->CCMR1;
    ccmr1 &= (uint16_t)~TIM_CCMR1_OC1M;
    ccmr1 &= (uint16_t)~TIM_CCMR1_CC1S;
    ccmr1 |= TIM_ForcedAction_InActive;

    TIM2->CCMR1 = ccmr1;
    TIM2->CCER = ccer;

    TIM_Cmd(TIM2, ENABLE);

    /* TIM2 enable counter */


    for(;;)
    {

         uint32_t tics = TIM_GetCounter(TIM2);

 //       etch_sequence(tics + 100000);
        //  pp_printf("td %d\n\r", is_touchdown());
        pp_printf("cnt %d irq %d lcnt %d\n\r", tics, irq_count, lcnt);
        delay(100);
    }
}