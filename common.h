#ifndef __COMMON_H__
#define __COMMON_H__

#define LOG_FILE RUNSTATEDIR "/charge-log"
#define UPDATE_INTERVAL 60

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

void read_energy(int * full, int * now);
int get_time();

#endif
