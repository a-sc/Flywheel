#ifndef __COMMON_H
#define __COMMON_H

uint32_t get_ticks_count();
void delay ( uint32_t how_much );

struct timeout {
  uint32_t last;
  uint32_t period;
};

static inline void tmo_init(struct timeout *tmo, uint32_t period)
{
  tmo->last = get_ticks_count();
  tmo->period = period;
}

static inline int tmo_hit(struct timeout *tmo)
{
  uint32_t t =  get_ticks_count();
  if(t - tmo->last >= tmo->period)
  {
    tmo->last = t;
    return 1;
  }
  return 0;
}


void etch_start(uint32_t start_at);
int etch_is_idle();
void etch_init();

#endif