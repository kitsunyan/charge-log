#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static int read_energy(int * full, int * now) {
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

static int get_time() {
	struct timespec time;
	clock_gettime(CLOCK_BOOTTIME, &time);
	return time.tv_nsec >= 500000000 ? time.tv_sec + 1 : time.tv_sec;
}

struct history_t {
	int time;
	int now;
};

static void history_add(struct history_t ** history, int * count, int * capacity,
	int time, int now) {
	if (*count == 0) {
		*capacity = 2;
		*history = malloc(*capacity * sizeof(struct history_t));
	} else if (*count >= *capacity) {
		*capacity *= 2;
		*history = realloc(*history, *capacity * sizeof(struct history_t));
	}
	(*history)[*count].time = time;
	(*history)[*count].now = now;
	(*count)++;
}

int main(int argc, char ** argv) {
	if (argc == 2 && !strcmp(argv[1], "daemon")) {
		FILE * file = fopen("/run/charge-log", "w");
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
				sleep(60);
			}
			fclose(file);
			return 0;
		}
	} else if (argc == 2 && !strcmp(argv[1], "plot")) {
		int fd[2];
		int pid;
		pipe(fd);
		pid = fork();
		if (pid < 0) {
			perror("Fork failed");
			return 1;
		} else if (pid == 0) {
			close(0);
			dup(fd[0]);
			close(fd[0]);
			close(fd[1]);
			execlp("gnuplot", "gnuplot", "-d", "-p", "-e",
				"set terminal qt size 700, 480 title 'Battery Charge History';"
				"set termoption font 'Workplace Sans,10';"
				"set margins 6, 4, 2, 3;"
				"show margin;"
				"unset mouse;"
				"set xdata time;"
				"set timefmt '%s';"
				"set format x '%H:%M';"
				"set grid xtics ytics mytics back;"
				"set multiplot layout 2, 1;"
				"set title 'Energy';"
				"set yrange [0:100];"
				"set ytics 20;"
				"set mytics 2;"
				"plot '-' using 1:2 with lines title '';"
				"unset yrange;"
				"set title 'Power';"
				"set yrange [0:];"
				"set ytics 3;"
				"set mytics 3;"
				"plot '-' using 1:2 with lines title '';"
				"unset multiplot;",
				NULL);
			return 1;
		} else {
			struct history_t * history = NULL;
			int history_count = 0;
			int history_capacity = 0;
			FILE * out = fdopen(fd[1], "w");
			FILE * file = fopen("/run/charge-log", "r");
			int efull, enow;
			int etime = get_time();
			int ctime;
			struct timespec ctime_full;
			int last_time = 0;
			close(fd[0]);
			tzset();
			clock_gettime(CLOCK_REALTIME, &ctime_full);
			ctime = ctime_full.tv_sec - timezone;
			if (file) {
				int time, full, now;
				while (fscanf(file, "%d %d %d\n", &time, &full, &now) == 3) {
					if (last_time >= time) {
						continue;
					}
					float value = 100. * now / full;
					value = value > 100 ? 100 : value < 0 ? 0 : value;
					fprintf(out, "%d %f\n", ctime - etime + time, value);
					history_add(&history, &history_count, &history_capacity,
						time, now);
					last_time = time;
				}
				fclose(file);
			}
			if (etime > last_time && read_energy(&efull, &enow)) {
				float value = 100. * enow / efull;
				value = value > 100 ? 100 : value < 0 ? 0 : value;
				fprintf(out, "%d %f\n", ctime, value);
				history_add(&history, &history_count, &history_capacity,
					etime, enow);
			}
			if (history_count <= 0) {
				fprintf(out, "0 0\n");
			}
			fprintf(out, "e\n");
			if (history_count <= 1) {
				fprintf(out, "0 0\n");
			} else {
				int i;
				for (i = 0; i < history_count; i++) {
					int step = history_count / 200;
					if (step < 1) {
						step = 1;
					}
					int first = i - step;
					int last = i + step;
					if (first < 0) {
						first = 0;
					}
					if (last >= history_count) {
						last = history_count - 1;
					}
					int td = history[last].time - history[first].time;
					int pd = history[last].now - history[first].now;
					float power = 0.0036 * pd / td;
					fprintf(out, "%d %f\n", ctime - etime + history[i].time,
						power < 0 ? -power : 0);
				}
			}
			fprintf(out, "e\n");
			fclose(out);
			if (history_capacity > 0) {
				free(history);
			}
			waitpid(pid, NULL, 0);
			return 0;
		}
	} else {
		FILE * file = argc == 1 ? stdout : stderr;
		fprintf(stderr,
			"Usage: charge-log MODE\n"
			"  daemon  record battery stats\n"
			"  plot    display a plot using gnuplot\n");
		return file == stdout ? 0 : 1;
	}
}
