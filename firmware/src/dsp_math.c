/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2010 - 2014 CERN (www.cern.ch)
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 * Author: Grzegorz Daniluk <grzegorz.daniluk@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */

/* spll_common.c - common data structures and functions used by the SoftPLL */

#include <string.h>
#include <stdint.h>

#include "dsp_math.h"

static int gen_dither( int pi_shift )
{
	static const uint32_t lcg_m = 1103515245;
	static const uint32_t lcg_i = 12345;	
  	static uint32_t seed = 0;

	seed *= lcg_m;
	seed += lcg_i;
	seed &= 0x7fffffffUL;

  	int d = seed & (( 1<<pi_shift) - 1);
	if ( seed & (1<<pi_shift))
		return -d;
	else
		return d;
	
}


int pi_update(pi_state_t *pi, int x)
{
	int64_t i_new;
	int y;
	pi->x = x;
	i_new = pi->integrator + (int64_t) pi->ki * x;

	int64_t y_preround = (i_new + (int64_t) x * pi->kp) + ( 1 << (pi->shift - 1) );

	int dither = pi->dithered ? gen_dither( pi->shift ) : 0;
	y = ( (y_preround + dither) >> pi->shift) + pi->bias;

	/* clamping (output has to be in <y_min, y_max>) and
	   anti-windup: stop the integrator if the output is already
	   out of range and the output is going further away from
	   y_min/y_max. */
	if (y < pi->y_min) {
		y = pi->y_min;
		if ((pi->anti_windup && (i_new > pi->integrator))
		    || !pi->anti_windup)
			pi->integrator = i_new;
	} else if (y > pi->y_max) {
		y = pi->y_max;
		if ((pi->anti_windup && (i_new < pi->integrator))
		    || !pi->anti_windup)
			pi->integrator = i_new;
	} else			/* No antiwindup/clamping? */
		pi->integrator = i_new;

	pi->y = y;
	return y;
}

void pi_init(pi_state_t *pi)
{
	pi->integrator = 0;
	pi->y = pi->bias;
	pi->dithered = 0;
}


void median_init( median_state_t *flt, int nsamples )
{
}

int median_update( median_state_t *flt, int x )
{
	return 0;
}

