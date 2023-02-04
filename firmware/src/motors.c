#if 0
#include "stm32f4xx.h"
#include "pp-printf.h"

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358
#endif

#define LDRIVE_IDLE 1
#define LDRIVE_FIND_HOMEZONE 2
#define LDRIVE_STEP 3
#define LDRIVE_GO_TO 4
#define LDRIVE_GO_HOME 5

#define HOME_ZONE_SIZE 64
#define HOME_ZONE_THRESHOLD 40.0

struct ldrive_encoder_state
{
    int i, q;
    float wrap;
    float ang_prev;
    int valid;
    float home_zone_log[HOME_ZONE_SIZE];
    int home_zone_samples;
    int last_dir;
};

struct ldrive_state
{
  int state;
  int next_state;
  int remaining_steps_out;
  int remaining_steps_in;
  int dir;
  int home_count;
  int step_tics;
  struct timeout step_timeout;
  struct ldrive_encoder_state enc;
};

struct ldrive_state ldrive;


struct esc_state {
  float target_speed;
  uint32_t update_count_last;
  struct pi_control pi;
  int strobe_angle_marks;
  int strobe_duration;
};

static struct esc_state esc;


int esc_pwm_init()
{
  TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
  TIM_OCInitTypeDef  TIM_OCInitStructure;

  uint16_t PrescalerValue = 0;

  GPIO_InitTypeDef GPIO_InitStructure;

  /* TIM3 clock enable */
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);

  /* GPIOC clock enable */
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);


  /* GPIOC Configuration: TIM3 CH1 (PC6), TIM3 CH2 (PC7), TIM3 CH3 (PC8) and TIM3 CH4 (PC9) */
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP ;
  GPIO_Init(GPIOC, &GPIO_InitStructure);

  /* Connect TIM3 pins to AF2 */
  GPIO_PinAFConfig(GPIOC, GPIO_PinSource6, GPIO_AF_TIM3);

  PrescalerValue = (uint16_t) ((SystemCoreClock ) / 2100000) - 1;

  /* Time base configuration */
  TIM_TimeBaseStructure.TIM_Period = 21000 * 20 / 10;
  TIM_TimeBaseStructure.TIM_Prescaler = PrescalerValue;
  TIM_TimeBaseStructure.TIM_ClockDivision = 0;
  TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;

  TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure);
  PrescalerValue = 1;

  /* PWM1 Mode configuration: Channel1 */
  TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
  TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
  TIM_OCInitStructure.TIM_Pulse = 100;
  TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;

  TIM_OC1Init(TIM3, &TIM_OCInitStructure);
  TIM_OC1PreloadConfig(TIM3, TIM_OCPreload_Enable);
  TIM_ARRPreloadConfig(TIM3, ENABLE);

  /* TIM3 enable counter */
  TIM_Cmd(TIM3, ENABLE);

  NVIC_InitTypeDef   NVIC_InitStructure;
  NVIC_InitStructure.NVIC_IRQChannel = TIM3_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x0;
  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0xf;
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
  NVIC_Init(&NVIC_InitStructure);


  return 0;
}


void esc_throttle_set(float value)
{
  TIM_OCInitTypeDef  TIM_OCInitStructure;

  int v = 2100 + (int)(value * 2100.0);

  pp_printf("Throttle: %d\n", v);

  /* PWM1 Mode configuration: Channel1 */
  TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
  TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
  TIM_OCInitStructure.TIM_Pulse = v;
  TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;

  TIM_OC1Init(TIM3, &TIM_OCInitStructure);

  TIM_OC1PreloadConfig(TIM3, TIM_OCPreload_Enable);

}

static void EXTILine12_Config(void)
{
  EXTI_InitTypeDef   EXTI_InitStructure;
  GPIO_InitTypeDef   GPIO_InitStructure;
  NVIC_InitTypeDef   NVIC_InitStructure;

  /* Enable GPIOA clock */
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
  /* Enable SYSCFG clock */
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);

  /* Configure PA0 pin as input floating */
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
  GPIO_Init(GPIOA, &GPIO_InitStructure);

  /* Connect EXTI Line0 to PA0 pin */
  SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOA, EXTI_PinSource12);

  /* Configure EXTI Line0 */
  EXTI_InitStructure.EXTI_Line = EXTI_Line12;
  EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
  EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising;
  EXTI_InitStructure.EXTI_LineCmd = ENABLE;
  EXTI_Init(&EXTI_InitStructure);

  /* Enable and set EXTI Line0 Interrupt to the lowest priority */
  NVIC_InitStructure.NVIC_IRQChannel = EXTI15_10_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x0F;
  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x0F;
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
  NVIC_Init(&NVIC_InitStructure);
}




void pi_init( struct pi_control *pi )
{
  pi->integrator = 0.0;
  pi->x0 = 0.0;
}

float pi_update( struct pi_control *pi, float x )
{
  pi->integrator += x;
  float y = pi->y_bias + pi->integrator * pi->ki + pi->kp * x;
  if(y > pi->y_max) y= pi->y_max;
  if(y < pi->y_min) y= pi->y_min;
  pi->x0 = x;

  return y;
}

struct pi_control esc_pi;

void esc_enable_power(int enable)
{
  if(enable)
    GPIO_SetBits(GPIOB, GPIO_Pin_1);
  else
    GPIO_ResetBits(GPIOB, GPIO_Pin_1);
}

void esc_init()
{
    GPIO_InitTypeDef  GPIO_InitStructure;

    TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_InitStructure.GPIO_Speed = GPIO_Fast_Speed;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    EXTILine12_Config();

    //RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

   #if 0
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP ;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
  #endif

    //GPIO_PinAFConfig(GPIOA, GPIO_PinSource0, GPIO_AF_TIM2);

    /* Connect TIM3 pins to AF2 */
    GPIO_PinAFConfig(GPIOC, GPIO_PinSource6, GPIO_AF_TIM3);


#if 0

  /* Time base configuration */
    TIM_TimeBaseStructure.TIM_Period = 0xffffffff;
    TIM_TimeBaseStructure.TIM_Prescaler = 0;
    TIM_TimeBaseStructure.TIM_ClockDivision = 0;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;

    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);
    TIM_OC1PreloadConfig(TIM2, TIM_OCPreload_Disable);

    /* TIM2 enable counter */
    TIM_Cmd(TIM2, ENABLE);

#endif

    esc.strobe_angle_marks = 0;
    esc.strobe_duration = 32; // 0..255

    esc.pi.ki = 0.00005;
    esc.pi.kp = 0.05;
    esc.pi.y_min = 0.0;
    esc.pi.y_max = 1.0;
    esc.pi.y_bias = 0.5;

    pi_init(&esc.pi);

    esc_pwm_init();
    pp_printf("PWM initialized\n\r");
    esc_throttle_set(0);
    delay(100);
    esc_enable_power(1);
    delay(2000);
    pp_printf("ESC initialized.\n\r");
}

int last_esc_tics_valid = 0;
uint32_t last_esc_tics = 0;

#define PULSE_FILTER_SIZE 32
#define PULSE_FILTER_MASK 0x1f

volatile int esc_pulse_filter [ PULSE_FILTER_SIZE ];
volatile int esc_wpos = 0;
volatile int esc_expected_value = 0;

volatile uint32_t last_tacho_ticks = 0;
volatile uint32_t esc_last_origin = 0;
volatile int esc_marks_per_turn = 0;
volatile int esc_mark_count = 0;

void esc_pulse_handler()
{
  uint32_t tics = TIM_GetCounter(TIM2);


  if(last_esc_tics_valid)
  {
    int delta;

    if ( tics < last_esc_tics )
      delta = last_esc_tics - tics;
    else
      delta = tics - last_esc_tics;

    if(delta > 4000) // ignore noise
    {
      esc_pulse_filter [ (esc_wpos++) & PULSE_FILTER_MASK ] = delta;

      if (delta > 5 * esc_expected_value / 4) // got full rotation marker?
      {
        esc_marks_per_turn = esc_mark_count;
        esc_mark_count = 0;
      }

      if(delta > esc_expected_value / 3)
      {
/*        TIM_OCInitTypeDef  TIM_OCInitStructure;

        TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_Inactive;
        TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
        TIM_OCInitStructure.TIM_Pulse = 0;
        TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;*/

        if(esc_mark_count == esc.strobe_angle_marks)
          GPIO_SetBits(GPIOA, GPIO_Pin_0);
        else
          GPIO_ResetBits(GPIOA, GPIO_Pin_0);

        esc_mark_count ++;
        esc_last_origin = tics;
      }
    }
  }


  last_tacho_ticks = get_ticks_count();

  last_esc_tics_valid = 1;
  last_esc_tics = tics;
}

void bsort(int *x, int n)
{
  int i,j;
  for(i=0;i<n;i++)
    for(j=0;j<n;j++)
      if(x[i]>x[j])
      {
        int tmp = x[i];
        x[i] = x[j];
        x[j] = tmp;
      }
}

int esc_compute_outliers()
{
  int pf[PULSE_FILTER_SIZE];

  memcpy(pf, (const void *)esc_pulse_filter, PULSE_FILTER_SIZE * sizeof(int));
  bsort(pf, PULSE_FILTER_SIZE);
  int median = pf[PULSE_FILTER_SIZE/2];
    int avg = 0, n = 0, i;



  for(i=0;i<PULSE_FILTER_SIZE;i++)
  {
    int ratio = (100 * abs(pf[i] - median)) / median;

    if( ratio < 5 )
    {
      avg += pf[i];
      n++;
    }
  }



  __disable_irq();

  if(n)
  {
    avg/=n;
    //pp_printf("avg %d n %d\n\r", avg, n );
    esc_expected_value = avg;
  } else {
    esc_expected_value = 10000000;
  }

  __enable_irq();


}



int esc_get_speed()
{
  esc_compute_outliers();

  return esc_expected_value;
}

#define MARKS_PER_TURN 179

float esc_get_speed_rps()
{
  esc_compute_outliers();


  int dt = get_ticks_count() - last_tacho_ticks;
  if(dt > 10)
    return 0.0;

  //pp_printf("dt %d\n\r", dt);

  if(esc_expected_value == 0)
    return 0.0;

  float v = 1.0f / ( (float)MARKS_PER_TURN * (float) esc_expected_value / (float) SystemCoreClock );
  return v;
}




void esc_set_speed ( float setpoint_rps )
{
  esc.target_speed = setpoint_rps;
}

#define ESC_UPDATE_PERIOD_MS 10 // ms

void esc_control_update()
{
#if 1
  uint32_t ticks = get_ticks_count();

  if(ticks - esc.update_count_last >= ESC_UPDATE_PERIOD_MS)
  {
    esc.update_count_last = ticks;

    if(esc.target_speed == 0.0)
    {
          esc_throttle_set(0);
    } else {

      float err = esc.target_speed - esc_get_speed_rps();

      float y = pi_update(&esc.pi, err);

      //pp_printf("speed %d err %d throttle %d orig %d pos %d\n\r", (int)esc_get_speed_rps(), (int)(1000.0*err), (int)(1000.0*y), esc_last_origin, esc_get_radial_position());

      //if(esc_target_speed>0)
      esc_throttle_set(y);
    }
  }
#endif

  //esc_throttle_set(0.3);
}


#if 0

void ldrive_update_encoder(struct ldrive_encoder_state *enc, int i, int q, int dir)
{
    enc->i = i;
    enc->q = q;
    float ang = atan2((float)i, (float)q) * 180.0 / M_PI;

    if(enc->valid)
    {
        float da = (ang - enc->ang_prev);

  if(da < -180.0)
      enc->wrap += 360.0;
  else if(da > 180.0)
      enc->wrap -= 360.0;
    }

    enc->ang_prev = ang;

//    if(dir != enc->last_dir)
  //  {
  //enc->home_zone_samples = 0;
   // }

    enc->home_zone_log[enc->home_zone_samples % HOME_ZONE_SIZE] = ang;
    enc->home_zone_samples++;

    enc->last_dir = dir;

    enc->valid = 1;
    // update the log

}

void ldrive_init_encoder(struct ldrive_encoder_state *enc)
{
    enc->wrap = 0.0;
    enc->valid = 0;
    enc->home_zone_samples = 0;
}

float ldrive_get_position(struct ldrive_encoder_state *enc)
{
    return enc->ang_prev + enc->wrap;
}

int ldrive_is_home_zone(struct ldrive_encoder_state *enc)
{
    if(enc->home_zone_samples < HOME_ZONE_SIZE)
      return 0;

    int i;

    float s = 0.0;

    for(i=0;i<HOME_ZONE_SIZE;i++)
      s += enc->home_zone_log[i];

    float max_d = 0.0;
    s /= (float) HOME_ZONE_SIZE;

    for(i=0;i<HOME_ZONE_SIZE;i++)
    {
      float delta = fabs(s - enc->home_zone_log[i]);
//      pp_printf("delta %d\n", (int)delta);
      if(delta > HOME_ZONE_THRESHOLD)
        return 0;
    }

    return 1;


}



void ldrive_init()
{
    GPIO_InitTypeDef  GPIO_InitStructure;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_InitStructure.GPIO_Speed = GPIO_Fast_Speed;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_InitStructure.GPIO_Speed = GPIO_Fast_Speed;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    GPIO_ResetBits(GPIOC, GPIO_Pin_5);
    GPIO_ResetBits(GPIOC, GPIO_Pin_9);

    ldrive.state = LDRIVE_IDLE;
    ldrive.home_count = 0;
    ldrive.remaining_steps_out = 70;
    ldrive.step_tics = 4;

    ldrive_init_encoder(&ldrive.enc);

}

void ldrive_step( int dir )
{

    if(dir)
      GPIO_ResetBits(GPIOC, GPIO_Pin_9);
    else
      GPIO_SetBits(GPIOC, GPIO_Pin_9);

    for(volatile int i =0;i<1000;i++);

    GPIO_SetBits(GPIOC, GPIO_Pin_5);
    for(volatile int i =0;i<1000;i++);
    GPIO_ResetBits(GPIOC, GPIO_Pin_5);
}



/*void test_tacho()
{
  pp_printf("TestTacho:\n\r");
  for(;;)
    {
      if(esc_expected_value > 0)
        pp_printf("Exp: %d\n\r", esc_expected_value);

      esc_control_update();
      delay(1);
    }
}*/

#endif

int esc_get_radial_position()
{
  uint32_t marks;
  uint32_t origin;
  uint32_t cnt;
  __disable_irq();
  marks = esc_mark_count;
  origin = esc_last_origin;
  cnt = TIM_GetCounter(TIM2);
  __enable_irq();


  return (esc_mark_count - 1) * 200 + ( 200LL * ((int64_t)cnt - origin) / esc_expected_value );
}

int esc_get_mark_count()
{
  return (esc_mark_count);
}

int inside_range(int value, int rmin, int rmax)
{
  return ( value >= rmin && value <= rmax );
}

#if 0

void ldrive_update()
{
  int dir = ldrive.remaining_steps_out ? 0 : 1;

  int enc_i = ldrive_get_encoder_value(0);
  int enc_q = ldrive_get_encoder_value(1);


  switch(ldrive.state)
  {
    case LDRIVE_IDLE:
      return;

    case LDRIVE_FIND_HOMEZONE:
    {
      ldrive.next_state = LDRIVE_FIND_HOMEZONE;
      ldrive.state = LDRIVE_STEP;

      tmo_init(&ldrive.step_timeout, ldrive.step_tics);
      ldrive_update_encoder(&ldrive.enc, enc_i, enc_q, 0);
      ldrive_step(dir);
      if(ldrive.remaining_steps_out)
      {
        ldrive.remaining_steps_out--;
        ldrive.enc.home_zone_samples = 0;
      }

//      pp_printf("rso %d i %d q %d ang %d dir %d is_h %d\n", ldrive.remaining_steps_out, enc_i, enc_q, (int)ldrive_get_position(&ldrive.enc), dir, ldrive_is_home_zone(&ldrive.enc));



      if( (!ldrive.remaining_steps_out) && ldrive_is_home_zone(&ldrive.enc) )
      {
  //      pp_printf("Home zone found\n");
        ldrive.state = LDRIVE_GO_HOME;
      }

      break;
    }

    case LDRIVE_GO_HOME:
    {
      ldrive.next_state = LDRIVE_GO_HOME;
      ldrive.state = LDRIVE_STEP;

      tmo_init(&ldrive.step_timeout, ldrive.step_tics);
      ldrive_update_encoder(&ldrive.enc, enc_i, enc_q, 0);
      ldrive_step(0);

      //pp_printf("rso %d i %d q %d and %d dir %d is_h %d\n", ldrive.remaining_steps_out, enc_i, enc_q, (int)ldrive_get_position(&ldrive.enc), dir, ldrive_is_home_zone(&ldrive.enc));


      if(!ldrive_is_home_zone(&ldrive.enc) )
      {
    //    pp_printf("Home zone edge found\n");

        ldrive.state = LDRIVE_IDLE;
      }

      break;
    }




    case LDRIVE_STEP:
      if(tmo_hit(&ldrive.step_timeout))
        ldrive.state = ldrive.next_state;
      break;

    case LDRIVE_GO_TO:
      if(ldrive.remaining_steps_out == 0 && ldrive.remaining_steps_in == 0)
      {
        ldrive.state = LDRIVE_IDLE;
        return;
      }

      tmo_init(&ldrive.step_timeout, ldrive.step_tics);

      ldrive.next_state = LDRIVE_GO_TO;
      ldrive.state = LDRIVE_STEP;

      ldrive_update_encoder(&ldrive.enc, enc_i, enc_q, 0);
      ldrive_step(ldrive.remaining_steps_out ? 0 : 1);

      if(ldrive.remaining_steps_out)
        ldrive.remaining_steps_out--;
      if(ldrive.remaining_steps_in)
        ldrive.remaining_steps_in--;

      //pp_printf("goto: steps-in %d steps-out %d\n\r", ldrive.remaining_steps_in, ldrive.remaining_steps_out);
      break;
  }
}

void ldrive_go_home()
{
  //pp_printf("GoHome!\n");
  ldrive.state = LDRIVE_FIND_HOMEZONE;
  ldrive.home_count = 0;
  ldrive.remaining_steps_out = 70;
  ldrive.step_tics = 5;
}

void ldrive_advance_by(int amount, int speed)
{
  ldrive.step_tics=  speed;

  if(amount < 0)
  {
    ldrive.remaining_steps_out = -amount;
    ldrive.remaining_steps_in = 0;
  } else {
    ldrive.remaining_steps_in = amount;
    ldrive.remaining_steps_out = 0;
  }
  ldrive.state = LDRIVE_GO_TO;
}

int ldrive_idle()
{
  return (ldrive.state == LDRIVE_IDLE) ? 1 : 0;
}

#endif
#endif