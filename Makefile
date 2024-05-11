CC = gcc
CFLAGS = -g -Wall

all: client server

client.o: client.c
	$(CC) $(CFLAGS) -c client.c

client: client.o
	$(CC) client.o -o client

server.o: server.c
	$(CC) $(CFLAGS) -c server.c

server: server.o
	$(CC) server.o -o server
clean:
	rm -f *.o client server
