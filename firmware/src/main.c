#include <stdio.h>
#include <string.h>


#include "stm32f4xx_conf.h"
#include "stm32f4xx.h"

#include "usart.h"
#include "common.h"
#include "bldc.h"


void clock_info()
{
    RCC_ClocksTypeDef rcc;
    RCC_GetClocksFreq(&rcc);

    printf("\r\nClock information:\r\n");
    printf("SWS: %d\n\r", rcc.sws);
    printf("SYSCLK: %d Hz\r\n", rcc.SYSCLK_Frequency);
    printf("AHB: %d Hz\r\n", rcc.HCLK_Frequency);
    printf("APB1: %d Hz\r\n", rcc.PCLK1_Frequency);
    printf("APB2: %d Hz\r\n", rcc.PCLK2_Frequency);
}


int main(void)
{
    SystemInit();
  
    SystemCoreClockUpdate();
    SysTick_Config(SystemCoreClock / 1000);

    GPIO_InitTypeDef  GPIO_InitStructure;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_InitStructure.GPIO_Speed = GPIO_Fast_Speed;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);

    usart_init();

    clock_info();
   
struct bldc_state bldc;
char command[100], arg1[100];
int speed_sp = 750;

 for(;;){
   printf("\r\nEnter command: ");
   scanf("%s", command);
   if (!strcmp(command, "start")) {
     bldc_init(&bldc);
     bldc.speed_setpoint = (speed_sp << 12)/60; 
     bldc.reverse_hall_phase = 1;
     bldc.hall_phase_advance = 1;
     delay(100);
     bldc_start(&bldc);
   }
   else if (!strcmp(command, "stop"))
     bldc_init(&bldc);
   else if (!strcmp(command, "set")) {
       scanf("%s %d", arg1, &speed_sp);
       if (!strcmp(arg1, "speed")) {
	 printf("\r\nRequested speed: %d rpm\r\n", speed_sp);
	 bldc.speed_setpoint = (speed_sp << 12) / 60;
       } else
	 printf("\r\nInvalid command\r\n");
   }
   else 
     printf("\r\nInvalid command\r\n");
 }

    return 0;
}


