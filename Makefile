.PHONY: clean install uninstall

CC		= clang
CFLAGS	= -Wall -g -std=gnu99

BUILD_DIR   = build
INCLUDE_DIR = include
SRC_DIR     = src
PREFIX      = /usr/local

TARGET = bamdb
LIBS   = -lhts -lm -llmdb -lpthread
INC    = -I $(INCLUDE_DIR)

OBJECTS = $(patsubst %.c, %.o, $(wildcard $(SRC_DIR)/*.c))
HEADERS = $(wildcard $(INCLUDE_DIR)/*.h)

%.o: %.c $(HEADERS)
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INC) -c $< -o $(patsubst src/%, $(BUILD_DIR)/%, $@)

BUILT_OBJECTS = $(patsubst src/%, $(BUILD_DIR)/%, $(OBJECTS))

$(TARGET): $(OBJECTS)
	$(CC) $(BUILT_OBJECTS) $(CFLAGS) $(INC) $(LIBS) -o $(BUILD_DIR)/$@

clean:
	-rm -f $(BUILD_DIR)/*.o
	-rm -f $(BUILD_DIR)/$(TARGET)
