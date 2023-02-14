/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2010 - 2013 CERN (www.cern.ch)
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */

/* spll_common.h - common data structures and functions used by the SoftPLL */

#ifndef __DSP_MATH_H
#define __DSP_MATH_H

/* PI regulator state */
typedef struct {
	int ki, kp;		/* integral and proportional gains (1<<PI_FRACBITS == 1.0f) */
	int shift;		/* fractional bits shift factor (defaults to PI_FRACBITS) */
	int64_t integrator;		/* current integrator value */
	int bias;		/* DC offset always added to the output */
	int anti_windup;	/* when non-zero, anti-windup is enabled */
	int y_min;		/* min/max output range, used by clapming and antiwindup algorithms */
	int y_max;
	int x, y;		/* Current input (x) and output value (y) */
	int dithered;		/* Enable dithering of DAC output */
} pi_state_t;

#define MAX_MEDIAN_SAMPLES 32

typedef struct {
	int buf[ MAX_MEDIAN_SAMPLES ];
	int sample_count;
	int median;
	int median_valid;
} median_state_t;


/* initializes the PI controller state. Currently almost a stub. */
void pi_init(pi_state_t *pi);

/* Processes a single sample (x) with PI control algorithm
 (pi). Returns the value (y) to drive the actuator. */
int pi_update(volatile pi_state_t *pi, int x);

void median_init( median_state_t *flt, int nsamples );

int median_update( median_state_t *flt, int x );

#endif


