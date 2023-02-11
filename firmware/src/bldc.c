#include <stdio.h>
#include <math.h>

#include "stm32f4xx.h"
#include "stm32f4xx_gpio.h"
#include "stm32f4xx_rcc.h"
#include "stm32f4xx_usart.h"
#include "stm32f4xx_adc.h"
#include "stm32f4xx_dma.h"
#include "stm32f4xx_tim.h"
#include "misc.h"

#include "bldc.h"
#include "dsp_math.h"

#define USE_TIM10_IRQ 0

static volatile struct bldc_state *g_bldc = NULL;

#if USE_TIM10_IRQ
// currently unused
void TIM1_UP_TIM10_IRQHandler(void)
{
	if (TIM_GetITStatus(TIM1, TIM_IT_Update) != RESET)
	{
		TIM_ClearITPendingBit(TIM1, TIM_IT_Update);
	}
}
#endif

static inline int bldc_read_hall_state(struct bldc_state *dev)
{
	uint32_t gpio = GPIO_ReadInputData(GPIOB);

	int hall_pos = (gpio & GPIO_Pin_6) ? 1 : 0;
	hall_pos |= (gpio & GPIO_Pin_8) ? 2 : 0;
	hall_pos |= (gpio & GPIO_Pin_9) ? 4 : 0;

	return hall_pos;
}

void bldc_hall_update_position(struct bldc_state *dev, int hall_pos)
{
	switch (hall_pos)
	{
	case 0b101:
		dev->hall_phase = 1;
		break;
	case 0b001:
		dev->hall_phase = 2;
		break;
	case 0b011:
		dev->hall_phase = 3; 
		break;
	case 0b010:
		dev->hall_phase = 4; 
		break;
	case 0b110:
		dev->hall_phase = 5;
		break;
	case 0b100:
		dev->hall_phase = 0;
		break;
	default:
		dev->hall_phase = 0; 
		break;
	}

	if (dev->hall_phase_d >= 0)
	{
		if (((dev->hall_phase_d + 1) % 6) != dev->hall_phase
		&& ((dev->hall_phase_d + 5) % 6) != dev->hall_phase )
		{
			dev->hall_step_missed++;
		}
	}

	dev->hall_phase_d = dev->hall_phase;
}

void bldc_pwm_init(struct bldc_state *dev)
{
	GPIO_InitTypeDef gpio_s;
	TIM_TimeBaseInitTypeDef tim_base_s;
	TIM_OCInitTypeDef tim_oc_s;
	TIM_BDTRInitTypeDef tim_bdtr_s;

	dev->pwm_period = BLDC_DEFAULT_CHOPPER_PERIOD;
	dev->pwm_duty = 3 * BLDC_DEFAULT_CHOPPER_PERIOD / 4;

	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE, ENABLE);

	// RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB, ENABLE);

	/* nucleo-f401RE PWM pin mapping:
	TIM1_CH1 = PA8 = CN10-23 = AF01
	//TIM1_CH1N = PA7 = CN10-15= AF01
	TIM1_CH2 = PA9 = CN10-21= AF01
	//TIM1_CH2N = PB0 = CN7-34= AF01
	TIM1_CH3 = PA10 = CN10-33= AF01
	//TIM1_CH3N = PB1 = CN10-24= AF01
	*/

	// initialize Tim1 PWM outputs
	gpio_s.GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10;
	gpio_s.GPIO_Speed = GPIO_Speed_50MHz;
	gpio_s.GPIO_Mode = GPIO_Mode_AF;
	gpio_s.GPIO_OType = GPIO_OType_PP;
	GPIO_Init(GPIOA, &gpio_s);

	gpio_s.GPIO_Pin = GPIO_Pin_7;
	gpio_s.GPIO_Speed = GPIO_Speed_50MHz;
	gpio_s.GPIO_Mode = GPIO_Mode_OUT;
	gpio_s.GPIO_OType = GPIO_OType_PP;
	GPIO_Init(GPIOA, &gpio_s);

	gpio_s.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1;
	gpio_s.GPIO_Speed = GPIO_Speed_50MHz;
	gpio_s.GPIO_Mode = GPIO_Mode_OUT;
	gpio_s.GPIO_OType = GPIO_OType_PP;
	GPIO_Init(GPIOB, &gpio_s);

	//	GPIO_PinAFConfig( GPIOA, GPIO_PinSource7, GPIO_AF_TIM1 );
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource8, GPIO_AF_TIM1);
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource9, GPIO_AF_TIM1);
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource10, GPIO_AF_TIM1);
	//	GPIO_PinAFConfig( GPIOB, GPIO_PinSource0, GPIO_AF_TIM1 );
	//	GPIO_PinAFConfig( GPIOB, GPIO_PinSource1, GPIO_AF_TIM1 );

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE);
	// Time Base configuration
	tim_base_s.TIM_Prescaler = 10;
	tim_base_s.TIM_CounterMode = TIM_CounterMode_Up;
	tim_base_s.TIM_Period = dev->pwm_period;
	tim_base_s.TIM_ClockDivision = 0;
	tim_base_s.TIM_RepetitionCounter = 0;
	TIM_TimeBaseInit(TIM1, &tim_base_s);

	// Channel 1, 2, 3 – set to PWM mode - all 6 outputs
	tim_oc_s.TIM_OCMode = TIM_OCMode_PWM2;
	tim_oc_s.TIM_OutputState = TIM_OutputState_Enable;
	tim_oc_s.TIM_OutputNState = TIM_OutputNState_Disable;

	tim_oc_s.TIM_OCPolarity = TIM_OCPolarity_Low;
	tim_oc_s.TIM_OCNPolarity = TIM_OCNPolarity_Low;
	tim_oc_s.TIM_OCIdleState = TIM_OCIdleState_Reset;
	tim_oc_s.TIM_OCNIdleState = TIM_OCNIdleState_Reset;

	tim_oc_s.TIM_Pulse = 0;
	TIM_OC1Init(TIM1, &tim_oc_s);
	tim_oc_s.TIM_Pulse = 0;
	TIM_OC2Init(TIM1, &tim_oc_s);
	tim_oc_s.TIM_Pulse = 0;
	TIM_OC3Init(TIM1, &tim_oc_s);

	// Update CCRx only on update event (now it's underflow)
	TIM_OC1PreloadConfig(TIM1, TIM_OCPreload_Disable);
	TIM_OC2PreloadConfig(TIM1, TIM_OCPreload_Disable);
	TIM_OC3PreloadConfig(TIM1, TIM_OCPreload_Disable);

	tim_bdtr_s.TIM_OSSRState = TIM_OSSRState_Disable;
	tim_bdtr_s.TIM_OSSIState = TIM_OSSIState_Disable;
	tim_bdtr_s.TIM_LOCKLevel = TIM_LOCKLevel_OFF;
	// DeadTime[ns] = value * (1/SystemCoreFreq) (on 72MHz: 7 is 98ns)
	tim_bdtr_s.TIM_DeadTime = 0;
	tim_bdtr_s.TIM_AutomaticOutput = TIM_AutomaticOutput_Disable;
	// Break functionality (overload input)
	// tim_bdtr_s.TIM_Break = TIM_Break_Enable;
	// tim_bdtr_s.TIM_BreakPolarity = TIM_BreakPolarity_Low;
	tim_bdtr_s.TIM_Break = TIM_Break_Disable;
	tim_bdtr_s.TIM_BreakPolarity = TIM_BreakPolarity_High;
	TIM_BDTRConfig(TIM1, &tim_bdtr_s);
	TIM_ClearFlag(TIM1, TIM_IT_Update);
	TIM_ITConfig(TIM1, TIM_IT_Update, ENABLE);

	// TIM_ClearFlag(TIM1, TIM_IT_CC1);
	// TIM_ITConfig(TIM1, TIM_IT_CC1, ENABLE);
        #if USE_TIM10_IRQ
	/* Enable the TIM1_IRQn Interrupt */
	NVIC_InitTypeDef nvic_s;
	nvic_s.NVIC_IRQChannel = TIM1_UP_TIM10_IRQn;
	// nvic_s.NVIC_IRQChannel = TIM1_CC_IRQn;
	nvic_s.NVIC_IRQChannelPreemptionPriority = 0;
	nvic_s.NVIC_IRQChannelSubPriority = 0;
	nvic_s.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&nvic_s);
	#endif

	TIM_Cmd(TIM1, ENABLE);
	// enable motor timer main output (the bridge signals)
	TIM_CtrlPWMOutputs(TIM1, ENABLE);
}

void EXTI9_5_IRQHandler(void)
{
	int s1 = EXTI_GetITStatus(EXTI_Line6);
	int s2 = EXTI_GetITStatus(EXTI_Line8);
	int s3 = EXTI_GetITStatus(EXTI_Line9);

	if (s1 || s2 || s3)
	{

		// Clear interrupt flags
		EXTI_ClearITPendingBit(EXTI_Line6);
		EXTI_ClearITPendingBit(EXTI_Line8);
		EXTI_ClearITPendingBit(EXTI_Line9);

		// Commutation
		// HallSensorsGetPosition();
		// commutate();

		if (g_bldc)
		{
			int hall_pos = bldc_read_hall_state(g_bldc);
			bldc_hall_update_position(g_bldc, hall_pos);
			bldc_commutate(g_bldc);
			int speed;
			if ((hall_pos & 1) && s1)
			{
			  speed = TIM_GetCounter(TIM3);
				// calc speed
			  if (TIM_GetITStatus(TIM3, TIM_IT_Update)) { 
			    speed = 65535;
			    TIM_ClearITPendingBit(TIM3, TIM_IT_Update);
			  }
				
				TIM_Cmd(TIM3, ENABLE);
				TIM_SetCounter(TIM3, 0);
				if( speed >= 0)
				{
					g_bldc->speed_raw_valid = 1;
					g_bldc->speed_raw = (int)(1000000ULL << 9) / speed;

					int err = g_bldc->speed_setpoint - g_bldc->speed_raw;

					g_bldc->pwm_duty = pi_update( &g_bldc->speed_pi, err );
				}
			}

			if (s1)
				g_bldc->hall_pulses[0]++;
			if (s2)
				g_bldc->hall_pulses[1]++;
			if (s3)
				g_bldc->hall_pulses[2]++;
		}
	}
}

void bldc_hall_init(struct bldc_state *dev)
{
	GPIO_InitTypeDef gpio_s;
	EXTI_InitTypeDef exti_s;
	NVIC_InitTypeDef nvic_s;

	dev->hall_phase = 0;
	dev->hall_pulses[0] = dev->hall_pulses[1] = dev->hall_pulses[2] = 0;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);

	// Init GPIO
	gpio_s.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_8 | GPIO_Pin_9;
	gpio_s.GPIO_Mode = GPIO_Mode_IN;
	gpio_s.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_Init(GPIOB, &gpio_s);

	// Init NVIC
	nvic_s.NVIC_IRQChannel = EXTI9_5_IRQn;
	nvic_s.NVIC_IRQChannelPreemptionPriority = 0x00;
	nvic_s.NVIC_IRQChannelSubPriority = 0x00;
	nvic_s.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&nvic_s);

	// Tell system that you will use EXTI_Lines */
	SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOB, GPIO_PinSource6);
	SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOB, GPIO_PinSource8);
	SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOB, GPIO_PinSource9);

	// EXTI
	exti_s.EXTI_Line = EXTI_Line6 | EXTI_Line8 | EXTI_Line9;
	exti_s.EXTI_LineCmd = ENABLE;
	exti_s.EXTI_Mode = EXTI_Mode_Interrupt;
	exti_s.EXTI_Trigger = EXTI_Trigger_Rising_Falling;
	EXTI_Init(&exti_s);
}

void bldc_init(struct bldc_state *dev)
{
	g_bldc = dev;
	memset(dev, 0, sizeof(struct bldc_state));

	dev->hall_phase_d = -1;
	dev->state = BLDC_STATE_OFF;

	bldc_pwm_init(dev);
	bldc_hall_init(dev);
	bldc_speed_timer_init(dev);
}

void bldc_start(struct bldc_state *dev)
{
	dev->speed_raw_valid = 0;
	dev->speed_raw = 0;
	dev->state = BLDC_STATE_ON;

	int hall_pos = bldc_read_hall_state(g_bldc);
	bldc_hall_update_position(dev, hall_pos);
	bldc_commutate(dev);

	pi_state_t *pi = &dev->speed_pi;

	pi->kp = 200; //10 << 12;
	pi->ki = 1; //1 << 12;
	pi->shift = 12;
	pi->dithered = 0;
	pi->y_min = 0;
	pi->y_max = dev->pwm_period-1;
	pi->bias = dev->pwm_period / 4;
	pi->anti_windup = 1;

	pi_init( pi );
}

// TIM1_CH1N = PA7 = CN10-15= AF01
// TIM1_CH2N = PB0 = CN7-34= AF01
// TIM1_CH3N = PB1 = CN10-24= AF01

#define PHASE_U 0x1
#define PHASE_V 0x2
#define PHASE_W 0x4

static inline void bldc_set_inhibit_n(struct bldc_state *dev, int mask_uvw)
{
	GPIO_WriteBit(GPIOA, GPIO_Pin_7, (mask_uvw & PHASE_U) ? 1 : 0);
	GPIO_WriteBit(GPIOB, GPIO_Pin_0, (mask_uvw & PHASE_V) ? 1 : 0);
	GPIO_WriteBit(GPIOB, GPIO_Pin_1, (mask_uvw & PHASE_W) ? 1 : 0);
}

static inline void bldc_set_pwm(struct bldc_state *dev, int phases, int duty)
{
	if( phases & PHASE_U )
		TIM1->CCR1 = duty;
	if( phases & PHASE_V )
		TIM1->CCR2 = duty;
	if( phases & PHASE_W )
		TIM1->CCR3 = duty;
}

void bldc_test()
{
	for(;;)
	{
		bldc_set_pwm(g_bldc, PHASE_W, 0 );
		delay(100);
		bldc_set_pwm(g_bldc, PHASE_W, 2500 );
		delay(100);
	}
			
}

void bldc_commutate(struct bldc_state *dev)
{
	#if 1

	int ph;
	
	if( dev->reverse_hall_phase )
		ph = 5 - dev->hall_phase;
	else
		ph = 8 - dev->hall_phase;

	ph += dev->hall_phase_advance;
	ph %= 6;

	if( dev->state == BLDC_STATE_OFF )
	{
		bldc_set_inhibit_n(dev, 0 );
		return;
	}

	 //= ((5-dev->hall_phase) + 1) % 6;
	 //pp_printf("COM %d\n", ph);

	 //return;
	switch (ph)
	{
		case 0:
			bldc_set_pwm(dev, PHASE_U | PHASE_V | PHASE_W, 0 );
			bldc_set_inhibit_n(dev, PHASE_V | PHASE_W );
			bldc_set_pwm(dev, PHASE_V, dev->pwm_duty);
			break;
		case 1:
			bldc_set_pwm(dev, PHASE_U | PHASE_V | PHASE_W, 0 );
			bldc_set_inhibit_n(dev, PHASE_U | PHASE_W );
			bldc_set_pwm(dev, PHASE_U, dev->pwm_duty);
			break;
		case 2:
			bldc_set_pwm(dev, PHASE_U | PHASE_V | PHASE_W, 0 );
			bldc_set_inhibit_n(dev, PHASE_U | PHASE_V );
			bldc_set_pwm(dev, PHASE_U, dev->pwm_duty);
			break;
		case 3:
			bldc_set_pwm(dev, PHASE_U | PHASE_V | PHASE_W, 0 );
			bldc_set_inhibit_n(dev, PHASE_V | PHASE_W );
			bldc_set_pwm(dev, PHASE_W, dev->pwm_duty);
			break;
		case 4:
			bldc_set_pwm(dev, PHASE_U | PHASE_V | PHASE_W, 0 );
			bldc_set_inhibit_n(dev, PHASE_U | PHASE_W);
			bldc_set_pwm(dev, PHASE_W, dev->pwm_duty);
			break;
		case 5:
			bldc_set_pwm(dev, PHASE_U | PHASE_V | PHASE_W, 0 );
			bldc_set_inhibit_n(dev, PHASE_U | PHASE_V);
			bldc_set_pwm(dev, PHASE_V, dev->pwm_duty);
			break;
	}

	
	#if 0
	switch (ph)
	{
	case 0: // Z+-
		bldc_set_inhibit_n(dev, PHASE_U | PHASE_V);
		bldc_set_pwm(dev, PHASE_U, 0);
		bldc_set_pwm(dev, PHASE_V, dev->pwm_duty);
		bldc_set_pwm(dev, PHASE_W, 0);
		break;
	case 1: // +Z-
		bldc_set_inhibit_n(dev, PHASE_V | PHASE_W);
		bldc_set_pwm(dev, PHASE_U, 0);
		bldc_set_pwm(dev, PHASE_V, dev->pwm_duty);
		bldc_set_pwm(dev, PHASE_W, 0);
		break;
	case 2: // +-Z
		bldc_set_inhibit_n(dev, PHASE_U | PHASE_W);
		bldc_set_pwm(dev, PHASE_U, dev->pwm_duty);
		bldc_set_pwm(dev, PHASE_V, 0);
		bldc_set_pwm(dev, PHASE_W, 0);
		break;
	case 3: // Z-+
		bldc_set_inhibit_n(dev, PHASE_U | PHASE_V);
		bldc_set_pwm(dev, PHASE_U, dev->pwm_duty);
		bldc_set_pwm(dev, PHASE_V, 0);
		bldc_set_pwm(dev, PHASE_W, 0);
		break;
	case 4: // -Z+
		bldc_set_inhibit_n(dev, PHASE_V | PHASE_W);
		bldc_set_pwm(dev, PHASE_U, 0);
		bldc_set_pwm(dev, PHASE_V, 0);
		bldc_set_pwm(dev, PHASE_W, dev->pwm_duty);
		break;
	case 5: // -+Z
		bldc_set_inhibit_n(dev, PHASE_U | PHASE_W);
		bldc_set_pwm(dev, PHASE_U, 0);
		bldc_set_pwm(dev, PHASE_V, 0);
		bldc_set_pwm(dev, PHASE_W, dev->pwm_duty);
		break;
	}
	#endif
	#endif
}

void bldc_speed_timer_init(struct bldc_state *dev)
{
	TIM_TimeBaseInitTypeDef timer_s;
	NVIC_InitTypeDef NVIC_InitStructure;

	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);

	TIM_TimeBaseStructInit(&timer_s);
	timer_s.TIM_CounterMode = TIM_CounterMode_Up;
	timer_s.TIM_Prescaler = BLDC_SPEED_TIMER_PRESCALER;
	timer_s.TIM_Period = BLDC_SPEED_TIMER_PERIOD;
	TIM_TimeBaseInit(TIM3, &timer_s);
	TIM_ITConfig(TIM3, TIM_IT_Update, ENABLE);
	TIM_SetCounter(TIM3, 0);
	TIM_Cmd(TIM3, ENABLE);

#if 0
	// NVIC Configuration
	// Enable the TIM3_IRQn Interrupt
	NVIC_InitStructure.NVIC_IRQChannel = TIM3_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 3;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
#endif
}
