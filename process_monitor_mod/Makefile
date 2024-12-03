CC = gcc
CFLAGS = -Wall -Wextra -g
LDFLAGS = -lncurses
OBJ = main.o process_handler.o ui.o utils.o
TARGET = process_monitor

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)

main.o: main.c global.h process_handler.h ui.h utils.h
process_handler.o: process_handler.c global.h process_handler.h
ui.o: ui.c global.h ui.h
utils.o: utils.c global.h utils.h

.PHONY: all clean
