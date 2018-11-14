# SPDX-License-Identifier: Beerware

CC := gcc
CFLAGS := -O2
LDFLAGS := -lm -lcrypto

SRCS = $(filter-out bin2c.c wtmi.c,$(wildcard *.c))
OBJS = $(patsubst %.c,%.o,$(SRCS))

all: mox-imager

clean:
	rm -f mox-imager $(OBJS) bin2c bin2c.o

mox-imager: $(OBJS)
	$(CC) $(CFLAGS) -o mox-imager $(OBJS) $(LDFLAGS)

mox-imager.c: wtmi.c
	touch mox-imager.c

bin2c: bin2c.o
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<
