CC		= gcc
CFLAGS	= -Wall -g -std=c99

all:
	mkdir -p bin/
	$(CC) $(CFLAGS) bamdb.c -L /usr/local/lib/ -o bin/bamdb -l hts -l m

clean:
	rm -f bin/bamdb
