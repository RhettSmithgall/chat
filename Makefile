CC = gcc

CFLAGS += -Wall -Wno-format-truncation

all: server client

server: server.c
	$(CC) $(CFLAGS) -o server server.c

client: ncurses_client.c
	$(CC) $(CFLAGS) -o client ncurses_client.c -lncurses

clean:
	rm -f server client
