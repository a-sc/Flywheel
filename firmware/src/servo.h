#ifndef __SERVO_H
#define __SERVO_H

#define SERVO_SAMPLE_RATE 21341
#define SERVO_LOG_SIZE 10000

struct servo_log_entry
{
  short setpoint;
  short error;
  short y;
} __attribute__((packed));

void servo_set_setpoint(int target);
void servo_init();

void servo_start_logging( int offset );
void servo_stop_logging( struct servo_log_entry **log, int *n_samples );
void servo_test_acquisition(int n_samples, int averaging, uint16_t *buf);
void servo_set_target(int target, int dvdt);
int servo_position_ready();
int servo_get_sensor();
void servo_all_back(int duration_samples);

void servo_heightmap_set(int mark, int height);
void servo_heightmap_enable(int enable, int hover_distance);

#endif
