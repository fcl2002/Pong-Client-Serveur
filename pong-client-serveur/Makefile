CC      = gcc -w
CFLAGS  = -Wall -Wextra -std=c11
LDFLAGS = -lm

BIN_DIR = bin

# TCP implementation
SERVER_TCP_SRC = server/server_tcp.c server/game.c
CLIENT_TCP_SRC = client/client_tcp.c

SERVER_TCP_BIN = $(BIN_DIR)/server_tcp
CLIENT_TCP_BIN = $(BIN_DIR)/client_tcp

# UDP implementation
SERVER_UDP_SRC = server/server_udp.c server/game.c
CLIENT_UDP_SRC = client/client_udp.c

SERVER_UDP_BIN = $(BIN_DIR)/server_udp
CLIENT_UDP_BIN = $(BIN_DIR)/client_udp

# Specific flags
CLIENT_CFLAGS = $(CFLAGS) -D_POSIX_C_SOURCE=200809L

.PHONY: all tcp udp server_tcp client_tcp server_udp client_udp \
        run_server_tcp run_client_tcp run_server_udp run_client_udp run_client_udp_p2 \
        clean re

# Build everything (TCP + UDP)
all: tcp udp

# Build TCP implementation
tcp: server_tcp client_tcp

# Build UDP implementation
udp: server_udp client_udp

# TCP targets
server_tcp: $(SERVER_TCP_BIN)
client_tcp: $(CLIENT_TCP_BIN)

# UDP targets
server_udp: $(SERVER_UDP_BIN)
client_udp: $(CLIENT_UDP_BIN)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# TCP binaries
$(SERVER_TCP_BIN): $(SERVER_TCP_SRC) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(SERVER_TCP_SRC) -o $(SERVER_TCP_BIN) $(LDFLAGS)

$(CLIENT_TCP_BIN): $(CLIENT_TCP_SRC) | $(BIN_DIR)
	$(CC) $(CLIENT_CFLAGS) $(CLIENT_TCP_SRC) -o $(CLIENT_TCP_BIN) $(LDFLAGS)

# UDP binaries
$(SERVER_UDP_BIN): $(SERVER_UDP_SRC) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(SERVER_UDP_SRC) -o $(SERVER_UDP_BIN) $(LDFLAGS)

$(CLIENT_UDP_BIN): $(CLIENT_UDP_SRC) | $(BIN_DIR)
	$(CC) $(CLIENT_CFLAGS) $(CLIENT_UDP_SRC) -o $(CLIENT_UDP_BIN) $(LDFLAGS)

# Run TCP server (port 8080)
run_server_tcp: $(SERVER_TCP_BIN)
	./$(SERVER_TCP_BIN) 8080

# Run TCP client
run_client_tcp: $(CLIENT_TCP_BIN)
	./$(CLIENT_TCP_BIN) 127.0.0.1 8080

# Run UDP server (default port 12345)
run_server_udp: $(SERVER_UDP_BIN)
	./$(SERVER_UDP_BIN)

# Run UDP client as player 0
run_client_udp: $(CLIENT_UDP_BIN)
	./$(CLIENT_UDP_BIN) 127.0.0.1 0

# Run UDP client as player 1
run_client_udp_p2: $(CLIENT_UDP_BIN)
	./$(CLIENT_UDP_BIN) 127.0.0.1 1

clean:
	rm -rf $(BIN_DIR)

re: clean all