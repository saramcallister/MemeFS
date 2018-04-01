CC=gcc
CFLAGS = -g -Og -Wall
PKGFLAGS = `pkg-config fuse --cflags --libs`
DEPS = blocklayer.h
OBJ = memefs.o blocklayer.o queue.o queue_desc.o

memefs: $(OBJ)
	gcc -o $@ $^ $(CFLAGS) $(PKGFLAGS)

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS) $(PKGFLAGS)

.PHONY: clean

clean:
	rm -f *.o
