#include <stdio.h>
#include <stdint.h>

#include <math.h>

#ifndef __TEST__
#include "pp-printf.h"
#endif

#define BIQUAD_FRACBITS 18

#define M_PI 3.14159265358

struct biquad
{
    int32_t a0, a1, a2, b1, b2;
    int32_t z1, z2;
};

#define BIQUAD_TYPE_PEAK 0
#define BIQUAD_TYPE_BANDPASS 1
#define BIQUAD_TYPE_LOWPASS 2


void biquad_init(struct biquad *flt, int type, double fsample, double fcenter, double Q, double peakGainDb)
{
    double Fc = fcenter / fsample;
    double norm, a0, a1, a2, b1, b2;
    double K = tan(M_PI * Fc);
    double sf = pow(2, BIQUAD_FRACBITS);
    double V = pow(10, fabs(peakGainDb) / 20.0);

//    printf("Fc %.10f\n", Fc);

    switch(type)
    {
	case BIQUAD_TYPE_BANDPASS:
	    norm = 1 / (1 + K / Q + K * K);
	    a0 = K / Q * norm;
	    a1 = 0;
	    a2 = -a0;
	    b1 = 2 * (K * K - 1) * norm;
	    b2 = (1 - K / Q + K * K) * norm;
	    break;
	case BIQUAD_TYPE_PEAK:
          if (peakGainDb >= 0) {    // boost
                norm = 1 / (1 + 1/Q * K + K * K);
                a0 = (1 + V/Q * K + K * K) * norm;
                a1 = 2 * (K * K - 1) * norm;
                a2 = (1 - V/Q * K + K * K) * norm;
                b1 = a1;
                b2 = (1 - 1/Q * K + K * K) * norm;
            }
            else {    // cut
                norm = 1 / (1 + V/Q * K + K * K);
                a0 = (1 + 1/Q * K + K * K) * norm;
                a1 = 2 * (K * K - 1) * norm;
                a2 = (1 - 1/Q * K + K * K) * norm;
                b1 = a1;
                b2 = (1 - V/Q * K + K * K) * norm;
            }
	    break;
	case BIQUAD_TYPE_LOWPASS:
	    norm = 1 / (1 + K / Q + K * K);
            a0 = K * K * norm;
            a1 = 2 * a0;
            a2 = a0;
            b1 = 2 * (K * K - 1) * norm;
            b2 = (1 - K / Q + K * K) * norm;

	default:
	    break;
    }

    flt->z1 = flt->z2 = 0;

/*    printf("a0: %.10f\n", a0);
    printf("a1: %.10f\n", a1);
    printf("a2: %.10f\n", a2);
    printf("b1: %.10f\n", b1);
    printf("b2: %.10f\n", b2);*/

    flt->a0 = (int32_t) (a0 * sf);
    flt->a1 = (int32_t) (a1 * sf);
    flt->a2 = (int32_t) (a2 * sf);
    flt->b1 = (int32_t) (b1 * sf);
    flt->b2 = (int32_t) (b2 * sf);

/*    pp_printf("a0: %d\n\r", flt->a0);
    pp_printf("a1: %d\n\r", flt->a1);
    pp_printf("a2: %d\n\r", flt->a2);
    pp_printf("b1: %d\n\r", flt->b1);
    pp_printf("b2: %d\n\r", flt->b2);*/

}

int32_t biquad_update( struct biquad *flt, int32_t x )
{

    int32_t out = (((int64_t)x * flt->a0) >> BIQUAD_FRACBITS) + flt->z1;
    flt->z1 = (((int64_t)x * flt->a1 - (int64_t)flt->b1 * out) >> BIQUAD_FRACBITS) + flt->z2;
    flt->z2 = ((int64_t)x * flt->a2 - (int64_t)flt->b2 * out) >> BIQUAD_FRACBITS;
    return out;
}

#ifdef __TEST__


void make_sine(double Fs, double Freq, int samples, int32_t *buf)
{
    int i;

    for(i=0;i<samples;i++)
    {
	buf[i] = 10000.0 * sin ( 2*M_PI*Freq/Fs * (double)i );
    }
}

void process_buffer(struct biquad *flt, int32_t *in, int32_t *out, int samples)
{
    int i;

    for(i=0;i<samples;i++)
    {
	out[i] = biquad_update(flt, in[i]);
    }
}

main()
{
    int buf_in[10000], buf_low[10000], buf_mid[10000], buf_hi[10000];

    struct biquad flt;

    double Fs = 40000.0;


    make_sine(Fs, 1000, 10000, buf_in);
    biquad_init(&flt, BIQUAD_TYPE_PEAK, Fs, 1000, 0.707, 0);
    process_buffer(&flt, buf_in, buf_mid, 10000);

    biquad_init(&flt,BIQUAD_TYPE_PEAK, Fs, 1000, 0.707, -3);
    process_buffer(&flt,  buf_in, buf_low, 10000);

    biquad_init(&flt,BIQUAD_TYPE_PEAK, Fs, 1000, 0.707, +3);
    process_buffer(&flt,  buf_in, buf_hi, 10000);

    FILE *f=fopen("Data.dat","wb");
int i;

for(i=0;i<10000;i++)
{
    fprintf(f,"%d %d %d %d %d\n", i, buf_in[i], buf_low[i], buf_mid[i], buf_hi[i]);
}
fclose(f);

}

#endif
