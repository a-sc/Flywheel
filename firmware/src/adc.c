#include "stm32f4xx.h"
#include "pp-printf.h"

#include "stm32f4xx_gpio.h"
#include "usart.h"

void adc_init()
{

  ADC_CommonInitTypeDef ADC_CommonInitStructure;

  /* Enable peripheral clocks *************************************************/
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA2, ENABLE);
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC2, ENABLE);

  /* DMA2 Stream0 channel0 configuration **************************************/
//  DMA_Config();

  /* ADCs configuration ------------------------------------------------------*/
  /* Configure ADC Channel10, 11, 12 pin as analog input */


  GPIO_InitTypeDef GPIO_InitStructure;
  /* ADC Channel 10 -> PC0
     ADC Channel 11 -> PC1
     ADC Channel 12 -> PC2
  */
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AN;
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL ;
  GPIO_Init(GPIOC, &GPIO_InitStructure);


  /* ADC Common Init */
  ADC_CommonInitStructure.ADC_Mode = ADC_DualMode_RegSimult;
  ADC_CommonInitStructure.ADC_Prescaler = ADC_Prescaler_Div2;
  ADC_CommonInitStructure.ADC_DMAAccessMode = ADC_DMAAccessMode_Disabled;
  ADC_CommonInitStructure.ADC_TwoSamplingDelay = ADC_TwoSamplingDelay_5Cycles;
  ADC_CommonInit(&ADC_CommonInitStructure);

  ADC_InitTypeDef ADC_InitStructure;

  ADC_InitStructure.ADC_Resolution = ADC_Resolution_12b;
  ADC_InitStructure.ADC_ScanConvMode = ENABLE;
  ADC_InitStructure.ADC_ContinuousConvMode = ENABLE;
  ADC_InitStructure.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_None;
  ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_T1_CC1;
  ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
  ADC_InitStructure.ADC_NbrOfConversion = 1;
  ADC_Init(ADC1, &ADC_InitStructure);

  /* ADC1 regular channels 10, 11 configuration */
  ADC_RegularChannelConfig(ADC1, ADC_Channel_10, 1, ADC_SampleTime_15Cycles);

/* Enable ADC1 */
  ADC_Cmd(ADC1, ENABLE);
  ADC_SoftwareStartConv(ADC1);


}

void adc_acquire(int n_samples, int *buf)
{
  /* Start ADC1 Software Conversion */

 int i;

  for(i = 0; i < n_samples; i++)
  {
      while(! ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) );
      ADC_ClearFlag(ADC1, ADC_FLAG_EOC);
      buf[i] = ADC_GetConversionValue(ADC1);
  }



}

void fet_charge(int on);
void fet_break(int on);


void hv_init()
{
    GPIO_InitTypeDef  GPIO_InitStructure;
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_InitStructure.GPIO_Speed = GPIO_Fast_Speed;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_InitStructure.GPIO_Speed = GPIO_Fast_Speed;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_InitStructure.GPIO_Speed = GPIO_Fast_Speed;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_InitStructure.GPIO_Speed = GPIO_Fast_Speed;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    fet_charge(0);
    fet_break(0);
}

void fet_charge(int on)
{
    if(on)
      GPIO_ResetBits(GPIOC, GPIO_Pin_10);
    else
      GPIO_SetBits(GPIOC, GPIO_Pin_10);
}

void strobe_led(int on)
{
    if(on)
      GPIO_SetBits(GPIOC, GPIO_Pin_10);
    else
      GPIO_ResetBits(GPIOC, GPIO_Pin_10);
}


void fet_break(int on)
{
    if(on)
      GPIO_ResetBits(GPIOC, GPIO_Pin_11);
    else
      GPIO_SetBits(GPIOC, GPIO_Pin_11);
}

int is_touchdown()
{
    return GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_12) ? 0: 1;
}

void hv_trigger(int t)
{
    if(t)
      GPIO_ResetBits(GPIOC, GPIO_Pin_3);
    else
      GPIO_SetBits(GPIOC, GPIO_Pin_3);
}


int arc_repeat = 1;
int arc_freq = 400;
int arc_duty = 30; // percent
int arc_delay = 50;

void arc_test()
{
    adc_init();
    hv_init();
    fet_charge(1);
    fet_break(0);

    for(;;)
    {

    int fire = 0;

    if (usart_poll())
    {
	int c = usart_rx_char();
	int new_setting = 1;

	switch(c)
	{
	    case 'a': if(arc_repeat < 20) arc_repeat++; break;
	    case 'z': if(arc_repeat > 1) arc_repeat--; break;
	    case 's': if(arc_freq < 400) arc_freq+=10; break;
	    case 'x': if(arc_freq > 20) arc_freq-=10; break;
	    case 'd': if(arc_duty < 80) arc_duty+=4; break;
	    case 'c': if(arc_duty > 0) arc_duty-=4; break;
	    case 'f': if(arc_delay < 100) arc_delay++; break;
	    case 'v': if(arc_delay > 4) arc_delay--; break;
	    case 'q': fire = 1; new_setting = 0; break;
	    default:
		new_setting = 0;
	}

	if(new_setting)
	    pp_printf("repeat %d freq %d duty %d delay %d\n\r", arc_repeat, arc_freq, arc_duty, arc_delay);
    }

    int r;

//    pp_printf("touch %d\n\r", is_touchdown());


    for(;;)
    {
	fet_break(1);
	delay(10);
	fet_break(0);
	delay(10);
    }

    if(fire)
    {
	pp_printf("Fire!\n\r");


	fet_charge(0);
	fet_break(1);

	delay(10);

        for(r=0;r<arc_repeat;r++)
	{
	    volatile int f1 = arc_freq * arc_duty / 100;
    	    volatile int d;

            hv_trigger(1);
    	    for(d=0;d<f1;d++);
    	    hv_trigger(0);
    	    fet_break(0);
//	    for(;d<arc_freq;d++);
	}

//	volatile int d;

//	for(d=0;d<1;d++);
	fet_break(0);


	delay(arc_delay);
	fet_charge(1);
    }
    }
}


float adc_measure()
{
  int n_samples = 16;
    int buf[1024];
    int i;

    float mean, dsum, std;
  //for(;;)
  {
    adc_acquire(n_samples, buf) ;

    mean = 0;
    dsum = 0;
    for(i=0;i<n_samples;i++)
      mean+=buf[i];

    mean /= (float)n_samples;

    for(i=0;i<n_samples;i++)
    {
      float delta= ((float)buf[i]-mean);
      dsum += delta*delta;
    }

    std = dsum / (float) n_samples;
    //std = sqrt(dsum / 1024.0);

    //pp_printf("mean %d std^2 %d\n\r", (int) mean, (int) std);

    return mean;


  }
}

#if 0
void adc_test()
{
  int buf[32];
    int i = 0;
  for(;;)
  {
//    adc_acquire(32, buf) ;

    hv_trigger(1);
    for(volatile int j=0;j<60;j++);
    hv_trigger(0);
//    for(volatile int j=0;j<70;j++);

    //for(i=0;i<1;i++)
      //pp_printf(" ", buf[i]);
//    pp_printf("Bang %d!\n\r", i++);
    delay(1);
  }

}
#endif