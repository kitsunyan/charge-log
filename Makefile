CC = gcc
CFLAGS =
LDFLAGS = -Wl,--as-needed
GTK = 3

ifeq ($(GTK), 3)
GTK_PC = gtk+-3.0
else ifeq ($(GTK), 2)
GTK_PC = gtk+-2.0
else
$(error GTK is invalid)
endif

EXTRA_CFLAGS = \
	-Wall -Wextra -Wno-unused-parameter \
	$(shell pkg-config --cflags $(GTK_PC))
EXTRA_LDFLAGS = \
	$(shell pkg-config --libs $(GTK_PC))

.PHONY: all clean

all: charge-log-daemon charge-log-plot

%.o: %.c common.h
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) -o $@ -c $<

charge-log-daemon: daemon.o common.o
	$(CC) -o $@ $^ $(LDFLAGS) $(EXTRA_LDFLAGS)

charge-log-plot: plot.o common.o
	$(CC) -o $@ $^ $(LDFLAGS) $(EXTRA_LDFLAGS) -lm

clean:
	rm -rfv *.o charge-log-daemon charge-log-plot
