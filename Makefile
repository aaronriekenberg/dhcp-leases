CC = cc
CFLAGS = -g -Wall
LDFLAGS =

SRC = main.c
OBJS = $(SRC:.c=.o)

all: dhcp-leases

clean:
	rm -f *.o dhcp-leases

dhcp-leases: $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o $@

depend:
	$(CC) $(CFLAGS) -MM $(SRC) > .makeinclude

include .makeinclude
