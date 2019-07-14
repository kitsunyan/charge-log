CC = gcc
CFLAGS =
LDFLAGS =
EXTRA_CFLAGS = -Wall -Wextra

.PHONY: all clean

all: charge-log

charge-log: charge-log.c
	gcc $(CFLAGS) $(EXTRA_CFLAGS) $(LDFLAGS) -o $@ $<

clean:
	rm -rfv charge-log
