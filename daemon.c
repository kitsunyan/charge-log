#include "common.h"

#include <stdio.h>
#include <unistd.h>

int main(int argc, char ** argv) {
	FILE * file = fopen(LOG_FILE, "w");
	if (!file) {
		perror("Failed to open a file");
		return 1;
	} else {
		while (1) {
			int full, now;
			int time = get_time();
			if (read_energy(&full, &now)) {
				fprintf(file, "%d %d %d\n", time, full, now);
				fflush(file);
			}
			sleep(UPDATE_INTERVAL);
		}
		fclose(file);
		return 0;
	}
}
