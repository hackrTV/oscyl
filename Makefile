CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -pedantic -I/usr/local/include
LDFLAGS = -L/usr/local/lib -lraylib -lFLAC -lvorbisfile -lm -lpthread -ldl

# Debug build by default
CFLAGS += -g -O0

SRC_DIR = src
BUILD_DIR = build

SRCS = $(SRC_DIR)/main.c $(SRC_DIR)/audio.c $(SRC_DIR)/playlist.c
OBJS = $(BUILD_DIR)/main.o $(BUILD_DIR)/audio.o $(BUILD_DIR)/playlist.o

TARGET = oscyl

.PHONY: all clean

all: $(BUILD_DIR) $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/main.o: $(SRC_DIR)/main.c $(SRC_DIR)/audio.h $(SRC_DIR)/playlist.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/audio.o: $(SRC_DIR)/audio.c $(SRC_DIR)/audio.h $(SRC_DIR)/miniaudio.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/playlist.o: $(SRC_DIR)/playlist.c $(SRC_DIR)/playlist.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) $(TARGET)
