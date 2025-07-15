CC = gcc
CFLAGS = -Wall -Wextra -pthread
SRV_DIR = server
CLI_DIR = client
COM_DIR = common
BIN_DIR = bin

all: server client

server: $(SRV_DIR)/battleserver.c $(COM_DIR)/protocol.h
	$(CC) $(CFLAGS) -o $(BIN_DIR)/battleserver $(SRV_DIR)/battleserver.c

client: $(CLI_DIR)/battleclient.c $(COM_DIR)/protocol.h
	$(CC) $(CFLAGS) -o $(BIN_DIR)/battleclient $(CLI_DIR)/battleclient.c

clean:
	rm -f $(BIN_DIR)/battleserver $(BIN_DIR)/battleclient

run_server:
	$(BIN_DIR)/battleserver

run_client:
	$(BIN_DIR)/battleclient
