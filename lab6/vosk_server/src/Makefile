
CC:=gcc

CFLAGS=-I.
LDFLAGS=-L.. -lvosk -ldl -lpthread -Wl,-rpath=.

all: server

server: server.o
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f *.o *.a server
