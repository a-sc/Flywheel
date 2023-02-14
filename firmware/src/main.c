#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "stm32f4xx_conf.h"
#include "stm32f4xx.h"

#include "pp-printf.h"
#include "biquad.h"
#include "usart.h"
#include "servo.h"
#include "rpc.h"
#include "motors.h"
#include "common.h"

#include "bldc.h"

void fet_charge(int on);
void fet_break(int on);
int is_touchdown();
void hv_init();
void esc_throttle_set(float value);

extern int __end__; /* From linker script */

int _write(int handle, char *data, int size ) 
{
    int count ;

    for( count = 0; count < size; count++) 
    {
      usart_send(data++, 1) ;  // Your low-level output function here.
    }

    return count;
}

int _fstat(int file, struct stat *st) {
  st->st_mode = S_IFCHR;

  return 0;
}

int _close(int file) {
  return -1;
}

int _lseek(int file, int ptr, int dir) {
  return 0;
}

int _read (int file, char * ptr, int len) {
  int read = 0;

  /*  if (file != 0) {
    return -1;
    }*/

  read = usart_recv(ptr, len, 1);
  return read;
}

int _isatty(int file) {
  return 1;
}



void *_sbrk(int incr) {
  static unsigned char *heap = NULL;
  unsigned char *prev_heap;

  if (heap == NULL) {
    heap = (unsigned char *)&__end__;
  }
  prev_heap = heap;

  heap += incr;

  return prev_heap;
}

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

//void esc_pulse_handler()
//{}

int wait_key()
{
  while(!usart_poll());
  return usart_rx_char();
}

#if 0
void test_esc()
{
  //esc_init();
    motors_init();

    //esc_throttle_set(0.3);
    //throttle_set(250);
    esc_set_speed(20.0);

    int n = 0, dn = 1;


//    arc_test();

    //esc_throttle_set(0.25);


    for(;;)
    {
      //EXTI_GenerateSWInterrupt(EXTI_Line3);
      esc_control_update();
      //pp_printf("Pulses: %d cnt %d\n\r", pcnt,  TIM_GetCounter(TIM2) );
      //pp_printf("n %d dn %d ESC Speed %d %d\n\r", n, dn, esc_get_speed(), (int) esc_get_speed_rps() );
      delay(10);

  #if 0
  n+=dn;

      if(dn > 0)
        head_step(1);
      else
        head_step(0);

      if( n == 100 || n == 0)
      {
        dn = -dn;
      }
      #endif

    }

}

void test_gpio_pin( void *gpio, int pin )
{
  GPIO_InitTypeDef GPIO_InitStructure;

  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);

  GPIO_InitStructure.GPIO_Pin = pin; // TIM4/PWM3
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL ;
  GPIO_Init(gpio, &GPIO_InitStructure);

  for(;;)
  {
    GPIO_SetBits(gpio, pin);
    delay(1);
    GPIO_ResetBits(gpio, pin);
    delay(1);
  }

}

int setpoint = 1000;

#if 1

void handle_kb()
{



    if (usart_poll())
    {
      int c = usart_rx_char();
      int new_setting = 1;

      switch(c)
      {
       /*   case 'z': if(notch1_center > 100) notch1_center -= 3.0; break;
          case 'a': if(notch1_center < 2000) notch1_center += 3.0;break;
          case 'c': if(notch1_center > 100) notch2_center -= 3.0; break;
          case 'd': if(notch1_center < 2000) notch2_center += 3.0;break;
          case 's': servo.gain_d1 += 5; break;
          case 'x': servo.gain_d1 -= 5; break;*/
          case 'f': setpoint += 5; break;
          case 'v': setpoint -= 5; break;
          default:
        new_setting = 0;
      }

    if(new_setting)
    {
      printf("sp: %d\n\r", setpoint);
      servo_set_setpoint(setpoint);
    }
  }

}
#endif

#endif

#if 0
uint16_t buf[8192];



void cmd_adc_test(struct rpc_request *rq)
{
  int n_samples = rpc_pop_int32(rq);
  int n_avg = rpc_pop_int32(rq);

  servo_test_acquisition(n_samples, n_avg, buf);
  rpc_answer_send(RPC_ID_ADC_TEST, 2 * n_samples, buf);

}

void cmd_esc_set_speed(struct rpc_request *rq)
{
  float speed = (float) rpc_pop_int32(rq) / 1000.0;
  printf("SetSpeed: %d\n", (int)speed);
  esc_set_speed( speed );
}

void cmd_esc_get_speed(struct rpc_request *rq)
{
  float speed = esc_get_speed_rps( );
  int speed_int = (int) ( speed * 1000.0 );

  rpc_answer_send(RPC_ID_GET_ESC_SPEED, 4, &speed_int);
}


void cmd_servo_response(struct rpc_request *rq)
{
  int init_setpoint = rpc_pop_int32(rq);
  int target_setpoint = rpc_pop_int32(rq);
  int dvdt = rpc_pop_int32(rq);
  struct servo_log_entry *log;
  int log_samples;

  servo_set_setpoint(init_setpoint);
  servo_start_logging( 0 );
  delay(200);
  int i;
  for(i=0;i<1;i++)
  {
  servo_set_target(target_setpoint, dvdt);
//  while(!servo_position_ready());
  delay(20);
  servo_set_target(init_setpoint, dvdt);
  //while(!servo_position_ready());
  delay(20);
  }
  delay(200);
  servo_stop_logging(&log, &log_samples);

//  pp_printf("log has %d samples\n\r", log_samples);
  rpc_answer_send(RPC_ID_SERVO_RESPONSE, log_samples * sizeof(struct servo_log_entry), log);
}


void cmd_profile_height(struct rpc_request *rq)
{
  int initial_setpoint = rpc_pop_int32(rq);
  int n_points = rpc_pop_int32(rq);
  printf("initial_setp %d np %d\n", initial_setpoint, n_points);
  servo_set_target(initial_setpoint, 1);
  while(!servo_position_ready());
  int i;
  delay(10);
  int p, r;



  for(i=0;i<n_points;i++)
  {
    esc_control_update();
    servo_set_target(300, 15);
    while(!is_touchdown());// pp_printf("target %d setp %d sen %d\n", servo_get_target(), servo_get_setpoint(), servo_get_sensor());


    p = servo_get_sensor();
    r = esc_get_radial_position();

    servo_set_setpoint(p + 300);
    while(is_touchdown());

//    delay(5);
    rpc_answer_push(&p, 4);
    rpc_answer_push(&r, 4);
    //delay(32);
    //pp_printf("touchdown at %d %d %d LO %d\n\r", p, servo_get_sensor(), servo_get_setpoint(), esc_get_radial_position());
  }

  rpc_answer_commit(RPC_ID_PROFILE_HEIGHT);
  servo_set_target(initial_setpoint, 1);

}

void cmd_ldrive_step(struct rpc_request *rq)
{
  int count = rpc_pop_int32(rq);

  //ldrive_step( dir );
  ldrive_advance_by(count, 10);
}

void cmd_ldrive_read_encoder(struct rpc_request *rq)
{
  int i = ldrive_get_encoder_value(0);
  int q = ldrive_get_encoder_value(1);


  rpc_answer_push(&i, 4);
  rpc_answer_push(&q, 4);

  rpc_answer_commit(RPC_ID_LDRIVE_READ_ENCODER);
}

void cmd_ldrive_go_home(struct rpc_request *rq)
{
  servo_set_setpoint(3000);
  ldrive_go_home();
}

void cmd_ldrive_check_idle(struct rpc_request *rq)
{
  int idle = ldrive_idle();
  rpc_answer_push(&idle, 4);
  rpc_answer_commit(RPC_ID_LDRIVE_CHECK_IDLE);
}

void cmd_servo_set_heightmap(struct rpc_request *rq)
{
  int i;
  for(i=0;i<180;i++)
  {
    int h=rpc_pop_int32(rq);
    printf("ang %d h %d\n\r", i, h);
    servo_heightmap_set(i, h);
  }
}

void cmd_servo_enable_heightmap(struct rpc_request *rq)
{
  int enable = rpc_pop_int32(rq);
  int height = rpc_pop_int32(rq);

  printf("hmap-enable %d %d\n\r",enable,height);
  if(!enable)
  {
    servo_set_setpoint(height);
  }

  servo_heightmap_enable(enable, height);


}

void cmd_etch_single_spot(struct rpc_request *rq)
{
  uint32_t t = TIM_GetCounter(TIM2);
  etch_start(t + 100000);
}



static int probe_single(int initial_setpoint)
{
  servo_set_target(initial_setpoint, 1);
  while(!servo_position_ready());
  servo_set_target(300, 15);
  while(!is_touchdown());// pp_printf("target %d setp %d sen %d\n", servo_get_target(), servo_get_setpoint(), servo_get_sensor());
  int p = servo_get_sensor();
  servo_set_setpoint(initial_setpoint);
  while(!servo_position_ready());
  return p;
}

void cmd_probe_and_etch(struct rpc_request *rq)
{
  int initial_setpoint = rpc_pop_int32(rq);
  int d = rpc_pop_int32(rq);

  int p = probe_single(initial_setpoint);

  printf("ProbeEtch:%d\n\r", p);
  servo_set_target(p+d, 15);
  while(!servo_position_ready());

  delay(300);

  uint32_t t = TIM_GetCounter(TIM2);
  etch_start(t + 100000);

}


void main_loop()
{
  struct rpc_request rq;
  struct timeout keepalive;

  tmo_init(&keepalive, 1000);
  while (1)
  {
    esc_control_update();
    ldrive_update();

    //pp_printf("touch %d\n\r", is_touchdown());
    if(tmo_hit(&keepalive))
    {
      printf("Heartbeat etch-irq %d\n", hv_irq_count());
      //pp_printf("servo %d %d\n\r", servo_get_sensor(), servo_get_setpoint());
    }

    if(!rpc_request_get(&rq))
      continue;


    switch(rq.id)
    {
      case RPC_ID_ADC_TEST: cmd_adc_test(&rq); break;
      case RPC_ID_GET_ESC_SPEED: cmd_esc_get_speed(&rq); break;
      case RPC_ID_SET_ESC_SPEED: cmd_esc_set_speed(&rq); break;
      case RPC_ID_SERVO_RESPONSE: cmd_servo_response(&rq); break;
      case RPC_ID_PROFILE_HEIGHT: cmd_profile_height(&rq); break;
      case RPC_ID_LDRIVE_STEP: cmd_ldrive_step(&rq); break;
      case RPC_ID_LDRIVE_READ_ENCODER: cmd_ldrive_read_encoder(&rq); break;
      case RPC_ID_LDRIVE_GO_HOME: cmd_ldrive_go_home(&rq); break;
      case RPC_ID_LDRIVE_CHECK_IDLE: cmd_ldrive_check_idle(&rq); break;
      case RPC_ID_SERVO_SET_HEIGHTMAP: cmd_servo_set_heightmap(&rq); break;
      case RPC_ID_SERVO_ENABLE_HEIGHTMAP: cmd_servo_enable_heightmap(&rq); break;
      case RPC_ID_ETCH_SINGLE_SPOT: cmd_etch_single_spot(&rq); break;
      case RPC_ID_PROBE_AND_ETCH: cmd_probe_and_etch(&rq); break;

      default: break;
    }
  }
}



#endif

extern volatile int tim3_irq_cnt;
extern volatile int sin_ctr;

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
   
  //for(;;);

    //servo_init();
    //ldrive_init();
//    etch_init();

   // bldc_init( TIM3, NULL );
   
struct bldc_state bldc;
 char command[100], arg1[100];
 int speed_sp = 750;

 for(;;){
   printf("\r\nEnter command: ");
   scanf("%s", command);
   // printf("scanf done\n");
   // printf("scanf read the following string: %s\n", command);
   if (!strcmp(command, "start")) {
     bldc_init(&bldc);
     bldc.speed_setpoint = (speed_sp << 12)/60; 
     bldc.reverse_hall_phase = 1;
     bldc.hall_phase_advance = 1;
     //printf("Start\n\r");
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
      //delay(10000);

  //bldc.state = BLDC_STATE_OFF;


  //bldc_test();
  
  for(;;)
  {
    printf("HM: %d hU %d hV %d hW %d phase %d speed %d valid %d duty %d speedSetP %d\n", bldc.hall_step_missed,
      bldc.hall_pulses[1], bldc.hall_pulses[2], bldc.hall_pulses[0], bldc.hall_phase,
      (bldc.speed_raw * 60) >> 12, bldc.speed_raw_valid, bldc.pwm_duty,
      (bldc.speed_setpoint * 60) >> 12
    );
    
    delay(500);
  }
    esc_init();
    float speed = 0.1;
    esc_throttle_set(0.5);
    printf("Crank up\n");
    delay(3000);
    printf("Crank up done\n");
    
    for(int i = 0; i < 100;i++)
    {
      esc_throttle_set(speed);
      speed += 0.01;
      delay(500);
      printf(".");
    }

    //esc_set_speed(10.0);
    //etch_init();


#if 0
    main_loop();

    ldrive_go_home();

    while(!ldrive_idle())
      ldrive_update();


    ldrive_advance_by(-500, 4);

    for(;;)
          ldrive_update();
#endif

  for(;;)
  {
    //pp_printf("Dupa!\n\r");
  }

    return 0;
}


