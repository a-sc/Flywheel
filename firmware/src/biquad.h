#ifndef __BIQUAD_H
#define __BIQUAD_H

#include <stdint.h>

#define BIQUAD_FRACBITS 28

struct biquad
{
    int32_t a0, a1, a2, b1, b2;
    int32_t z1, z2;
};

#define BIQUAD_TYPE_PEAK 0
#define BIQUAD_TYPE_BANDPASS 1
#define BIQUAD_TYPE_LOWPASS 2

void biquad_init(struct biquad *flt, int type, double fsample, double fcenter, double Q, double peakGainDb);
int32_t biquad_update( struct biquad *flt, int32_t x );

#endif
