CC = cc
CFLAGS = -g -Wall
LDFLAGS =

MAKE_DB_SRC = make-db.c
MAKE_DB_OBJS = $(MAKE_DB_SRC:.c=.o)

SHOW_LEASES_SRC = show-leases.c
SHOW_LEASES_OBJS = $(SHOW_LEASES_SRC:.c=.o)

all: make-db show-leases

clean:
	rm -f *.o make-db show-leases

make-db: $(MAKE_DB_OBJS)
	$(CC) $(LDFLAGS) $(MAKE_DB_OBJS) -o $@

show-leases: $(SHOW_LEASES_OBJS)
	$(CC) $(LDFLAGS) $(SHOW_LEASES_OBJS) -o $@

depend:
	$(CC) $(CFLAGS) -MM $(MAKE_DB_SRC) $(SHOW_LEASES_SRC) > .makeinclude

include .makeinclude
