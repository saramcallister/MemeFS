CC=gcc
CFLAGS=-I.
DEPS = blocklayer.h
OBJ = memefs.o blocklayer.o queue.o queue_desc.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

memefs: $(OBJ)
	gcc -o $@ $^ $(CFLAGS)

.PHONY: clean

clean:
	rm -f *.o 
