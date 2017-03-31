CC		= clang
CFLAGS	= -Wall -g -std=gnu99

SOURCES	:= src/bam_api.c src/bam_lmdb.c
LIB		:= -lhts -lm -llmdb -lpthread
INC		:= -I include

all:
	@mkdir -p bin/
	$(CC) $(CFLAGS) $(SOURCES) bamdb.c -L /usr/local -o bin/bamdb $(LIB) $(INC)

clean:
	rm -f bin/bamdb
