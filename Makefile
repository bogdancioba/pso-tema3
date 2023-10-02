CC = gcc
CFLAGS = -Wall

all: client server

client: client.o
	$(CC) $(CFLAGS) -o client client.o

server: file_server.o
	$(CC) $(CFLAGS) -o server file_server.o

client.o: client.c
	$(CC) $(CFLAGS) -c client.c

file_server.o: file_server.c
	$(CC) $(CFLAGS) -c file_server.c

clean:
	rm -f *.o client server
