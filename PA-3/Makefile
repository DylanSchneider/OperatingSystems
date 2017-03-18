CC = gcc
CFLAGS = -c -Wall -Wextra
LFLAGS = -Wall -Wextra -pthread

.PHONY: all clean

all: multi-threadedDNS

multi-threadedDNS: multi-threadedDNS.o queue.o util.o
	$(CC) $(LFLAGS) $^ -o $@

multi-threadedDNS.o: multi-threadedDNS.c
	$(CC) $(CFLAGS) $<

queue.o: queue.c queue.h
	$(CC) $(CFLAGS) $<

util.o: util.c util.h
	$(CC) $(CFLAGS) $<

clean:
	rm -f multi-threadedDNS
	rm -f *.o
	rm -f *~
	rm -f out.txt
