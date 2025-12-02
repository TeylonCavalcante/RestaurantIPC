CC = gcc
CFLAGS = -Wall -O2
LDFLAGS = -lrt -pthread
SRC = src

all: server client

server: $(SRC)/server.c $(SRC)/common.h
	$(CC) $(CFLAGS) $(SRC)/server.c -o server $(LDFLAGS)

client: $(SRC)/client.c $(SRC)/common.h
	$(CC) $(CFLAGS) $(SRC)/client.c -o client $(LDFLAGS)

clean:
	rm -f server client pedidos.log
