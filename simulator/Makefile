CC = gcc
CFLAGS = -Wall -Wextra -O2 -I..

all: tcp_client tcp_server

tcp_client: tcp_client.c
	$(CC) $(CFLAGS) -o tcp_client tcp_client.c

tcp_server: tcp_server.c
	$(CC) $(CFLAGS) -o tcp_server tcp_server.c

clean:
	rm -f tcp_client tcp_server

.PHONY: all clean
