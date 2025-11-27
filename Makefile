CC = gcc
CFLAGS = -Wall -Werror -g

SRC_DIR = ./src
BIN_DIR = ./bin

OUTPUT = $(BIN_DIR)/tftp-client-g204
SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(BIN_DIR)/%.o)

all: $(OUTPUT)

$(BIN_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTPUT): $(OBJS)
	$(CC) $(OBJS) -o $(OUTPUT)

clean:
	rm -f $(BIN_DIR)/*.o $(OUTPUT)

.PHONY: all clean
