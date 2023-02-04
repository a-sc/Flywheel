#ifndef __BLDC_H
#define __BLDC_H

#include <stdint.h>

#include "dsp_math.h"

#define BLDC_DEFAULT_CHOPPER_PERIOD 1000

#define BLDC_SPEED_TIMER_PRESCALER 84
#define BLDC_SPEED_TIMER_PERIOD 0xFFFF // 65535

#define BLDC_STATE_OFF 0
#define BLDC_STATE_RAMP_UP 1
#define BLDC_STATE_ON 2

struct bldc_state
{
	int hall_phase;
	int hall_phase_d;
	int hall_step_missed;
    int pwm_duty;
	int pwm_period;
    int sensor_speed;
	int sensor_pos;
	int hall_pulses[3];
	int speed_raw;
	int speed_raw_valid;
	int speed_setpoint;
	int reverse_hall_phase;
	int hall_phase_advance;
	pi_state_t speed_pi;
	int state;
};

void bldc_init( struct bldc_state *dev );

#endif
