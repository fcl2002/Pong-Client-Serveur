CC = gcc
CFLAGS = -Wall -Wextra -std=c11
LDFLAGS = -lm

all: server client

server:
	mkdir -p bin
	$(CC) $(CFLAGS) server/server_tcp.c server/game.c -o bin/server_tcp $(LDFLAGS)

client:
	mkdir -p bin
	$(CC) $(CFLAGS) client/client_tcp.c -o bin/client_tcp

run_server_tcp: server_tcp
	./bin/server_tcp 8080

run_client_tcp: client_tcp
	./bin/client_tcp 127.0.0.1 8080

clean:
	rm -rf bin

re: clean all
