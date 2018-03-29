CC = gcc
CFLAGS = -Wall -Wextra -lcurl


all: blocklayer.o

blocklayer.o: blocklayer.c blocklayer.h memedl.o queue.o queue_desc.o
	$(CC) -c blocklayer.c $(CFLAGS)

memedl.o: memedl.c getinmemory.o urlextract.o url2file.o string_queue.o
	$(CC) -c memedl.c $(CFLAGS)

getinmemory.o: getinmemory.c
	$(CC) -c getinmemory.c $(CFLAGS)

urlextract.o: urlextract.c
	$(CC) -c urlextract.c $(CFLAGS)

url2file.o: url2file.c
	$(CC) -c url2file.c $(CFLAGS)

queue.o: queue.c
	$(CC) -c queue.c $(CFLAGS)

queue_desc.o: queue_desc.c
	$(CC) -c queue_desc.c $(CFLAGS)

string_queue.o: string_queue.c
	$(CC) -c string_queue.c $(CFLAGS)

clean:
	rm *.o
