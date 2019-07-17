#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define ENERGY_DECLARE(out) int out = 0; int out##_present = 0;

#define ENERGY_COMPARE(what, out) \
if (strstr(buf, #what "=") == buf) { \
	out = atoi(&strstr(buf, "=")[1]); \
	out##_present = 1; \
	continue; \
}

#define ENERGY_PRESENT_MOVE(p, np) \
if (!np##_present && p##_present) { \
	np##_present = 1; \
	np = p; \
}

void read_energy(int * full, int * now) {
        char buf[80];
	int i;
	*full = 0;
	*now = 0;

	for (i = 0;; i++) {
		ENERGY_DECLARE(energy_full);
		ENERGY_DECLARE(energy_full_design);
		ENERGY_DECLARE(energy_now);
		ENERGY_DECLARE(energy_avg);
		ENERGY_DECLARE(voltage_max_design);
		ENERGY_DECLARE(voltage_min_design);
		ENERGY_DECLARE(voltage_now);
		ENERGY_DECLARE(charge_full);
		ENERGY_DECLARE(charge_full_design);
		ENERGY_DECLARE(charge_now);
		ENERGY_DECLARE(charge_avg);
	        FILE * file;

		sprintf(buf, "/sys/class/power_supply/BAT%d/uevent", i);
		file = fopen(buf, "r");
	        if (file) {
	                while (fscanf(file, "POWER_SUPPLY_%s\n", buf) == 1) {
				ENERGY_COMPARE(ENERGY_FULL, energy_full);
				ENERGY_COMPARE(ENERGY_FULL_DESIGN, energy_full_design);
				ENERGY_COMPARE(ENERGY_NOW, energy_now);
				ENERGY_COMPARE(ENERGY_AVG, energy_avg);
				ENERGY_COMPARE(VOLTAGE_MAX_DESIGN, voltage_max_design);
				ENERGY_COMPARE(VOLTAGE_MIN_DESIGN, voltage_min_design);
				ENERGY_COMPARE(VOLTAGE_NOW, voltage_now);
				ENERGY_COMPARE(CHARGE_FULL, charge_full);
				ENERGY_COMPARE(CHARGE_FULL_DESIGN, charge_full_design);
				ENERGY_COMPARE(CHARGE_NOW, charge_now);
				ENERGY_COMPARE(CHARGE_AVG, charge_avg);
	                }
	                fclose(file);
	        } else {
			break;
		}

		ENERGY_PRESENT_MOVE(energy_full_design, energy_full);
		ENERGY_PRESENT_MOVE(energy_avg, energy_now);

		if (energy_full_present && energy_now_present) {
			(*full) += energy_full;
			(*now) += energy_now;
			continue;
		}

		ENERGY_PRESENT_MOVE(voltage_min_design, voltage_max_design);
		ENERGY_PRESENT_MOVE(voltage_now, voltage_max_design);
		ENERGY_PRESENT_MOVE(charge_full_design, charge_full);
		ENERGY_PRESENT_MOVE(charge_avg, charge_now);

		if (voltage_max_design_present && charge_full_present && charge_now_present) {
			(*full) += (voltage_max_design / 1000) * (charge_full / 1000);
			(*now) += (voltage_max_design / 1000) * (charge_now / 1000);
			continue;
		}
	}
}

int get_time() {
        struct timespec time;
        clock_gettime(CLOCK_BOOTTIME, &time);
        return time.tv_nsec >= 500000000 ? time.tv_sec + 1 : time.tv_sec;
}
