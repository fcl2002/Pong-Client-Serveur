CC      = gcc -w
CFLAGS  = -Wall -Wextra -std=c11
LDFLAGS = -lm

BIN_DIR = bin

SERVER_SRC = server/server_tcp.c server/game.c
CLIENT_SRC = client/client_tcp.c

SERVER_BIN = $(BIN_DIR)/server_tcp
CLIENT_BIN = $(BIN_DIR)/client_tcp

# Flags específicas
CLIENT_CFLAGS = $(CFLAGS) -D_POSIX_C_SOURCE=200809L

.PHONY: all server client run_server_tcp run_client_tcp clean re

all: server client

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

server: $(SERVER_BIN)
client: $(CLIENT_BIN)

$(SERVER_BIN): $(SERVER_SRC) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(SERVER_SRC) -o $(SERVER_BIN) $(LDFLAGS)

$(CLIENT_BIN): $(CLIENT_SRC) | $(BIN_DIR)
	$(CC) $(CLIENT_CFLAGS) $(CLIENT_SRC) -o $(CLIENT_BIN) $(LDFLAGS)

run_server_tcp: $(SERVER_BIN)
	./$(SERVER_BIN) 8080

run_client_tcp: $(CLIENT_BIN)
	./$(CLIENT_BIN) 127.0.0.1 8080

clean:
	rm -rf $(BIN_DIR)

re: clean all
