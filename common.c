#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int read_energy(int * full, int * now) {
        char buf[80];
        int has_full = 0;
        int has_now = 0;
        FILE * file = fopen("/sys/class/power_supply/BAT0/uevent", "r");
        if (file) {
                while (fscanf(file, "POWER_SUPPLY_%s\n", buf) == 1) {
                        int * out = NULL;
                        if (strstr(buf, "ENERGY_FULL=") == buf) {
                                out = full;
                                has_full = 1;
                        } else if (strstr(buf, "ENERGY_NOW=") == buf) {
                                out = now;
                                has_now = 1;
                        }
                        if (out) {
                                *out = atoi(&strstr(buf, "=")[1]);
                                if (has_full && has_now) {
                                        break;
                                }
                        }
                }
                fclose(file);
        }
        if (!has_full) {
                fprintf(stderr, "Failed to extract ENERGY_FULL\n");
        }
        if (!has_now) {
                fprintf(stderr, "Failed to extract ENERGY_NOW\n");
        }
        return has_full && has_now;
}

int get_time() {
        struct timespec time;
        clock_gettime(CLOCK_BOOTTIME, &time);
        return time.tv_nsec >= 500000000 ? time.tv_sec + 1 : time.tv_sec;
}
