.PHONY: clean install uninstall

CC		= clang
CFLAGS	= -Wall -g -std=gnu99 -fPIC
LDFLAGS = -shared

BUILD_DIR   = build
INCLUDE_DIR = include
SRC_DIR     = src
PREFIX      = /usr/local

TARGET   = bamdb
ALIB     = libbamdb.a
SLIB     = libbamdb.so
INC_LIBS = -lhts -lm -llmdb -lpthread
INC      = -I $(INCLUDE_DIR)

OBJECTS = $(patsubst %.c, %.o, $(wildcard $(SRC_DIR)/*.c))
HEADERS = $(wildcard $(INCLUDE_DIR)/*.h)

%.o: %.c $(HEADERS)
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INC) -c $< -o $(patsubst src/%, $(BUILD_DIR)/%, $@)

BUILT_OBJECTS = $(patsubst src/%, $(BUILD_DIR)/%, $(OBJECTS))

$(TARGET): $(OBJECTS)
	$(CC) $(BUILT_OBJECTS) $(CFLAGS) $(INC) $(INC_LIBS) -o $(BUILD_DIR)/$@

$(ALIB): $(BUILT_OBJECTS)
	$(AR) rcs $(BUILD_DIR)/$@ $^

$(SLIB): $(BUILT_OBJECTS)
	$(CC) $(CFLAGS) $(INC_LIBS) -o $(SLIB) $(BUILT_OBJECTS)
	clang -Wall -g -std=gnu99 -fPIC -lhts -lm -llmdb -lpthread -o build/libbamdb.so  build/bam_lmdb.o  build/bam_api.o  build/bamdb.o -shared

install: $(ALIB) $(SLIB)
	mkdir -p $(DESTDIR)$(PREFIX)/lib
	mkdir -p $(DESTDIR)$(PREFIX)/include
	cp $(BUILD_DIR)/$(ALIB) $(DESTDIR)$(PREFIX)/lib/$(ALIB)
	cp $(BUILD_DIR)/$(SLIB) $(DESTDIR)$(PREFIX)/lib/$(SLIB)
	cp $(HEADERS) $(DESTDIR)$(PREFIX)/include/

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/lib/$(ALIB)
	# TODO: autopopulate this list
	rm -f $(DESTDIR)$(PREFIX)/include/bam_api.h
	rm -f $(DESTDIR)$(PREFIX)/include/bam_lmdb.h
	rm -f $(DESTDIR)$(PREFIX)/include/bamdb.h

clean:
	-rm -f $(BUILD_DIR)/*.o
	-rm -f $(BUILD_DIR)/$(TARGET)
	-rm -f $(BUILD_DIR)/$(ALIB)
