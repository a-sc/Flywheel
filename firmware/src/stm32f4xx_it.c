#include <stdint.h>

#include "stm32f4xx.h"
#include "stm32f4xx_it.h"

extern void esc_pulse_handler();

void NMI_Handler(void)
{
}

void HardFault_Handler(void)
{
    /* Go to infinite loop when Hard Fault exception occurs */
    while (1)
    {
    }
}

void MemManage_Handler(void)
{
    /* Go to infinite loop when Memory Manage exception occurs */
    while (1)
    {
    }
}

void BusFault_Handler(void)
{
    /* Go to infinite loop when Bus Fault exception occurs */
    while (1)
    {
    }
}

void UsageFault_Handler(void)
{
    /* Go to infinite loop when Usage Fault exception occurs */
    while (1)
    {
    }
}

volatile int pcnt = 0;

void EXTI15_10_IRQHandler(void)
{
  if(EXTI_GetITStatus(EXTI_Line12) != RESET)
  {
    pcnt++;
    //esc_pulse_handler();

    EXTI_ClearITPendingBit(EXTI_Line12);
  }
}



void SVC_Handler(void)
{
}

void DebugMon_Handler(void)
{
}

void PendSV_Handler(void)
{
}

static volatile uint32_t delay_count = 0, tick_count = 0;

void SysTick_Handler(void)
{
    if(delay_count)
        delay_count--;
    tick_count++;
}

void OTG_FS_IRQHandler(void)
{
}


void delay ( uint32_t how_much )
{
    delay_count = how_much;
    while(delay_count);
}

uint32_t get_ticks_count()
{
    return tick_count;
}


