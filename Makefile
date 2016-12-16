CC		= gcc
CFLAGS	= -Wall -g -std=c99

all:
	$(CC) $(CFLAGS) bamdb.c -L /usr/local/lib/ -o bamdb -l hts -l m

clean:
	rm -f bamdb
