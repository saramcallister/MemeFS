CC=gcc
CFLAGS = -g -Og -Wall -Wextra
PKGFLAGS = `pkg-config fuse --cflags --libs` -lcurl
DEPS = blocklayer.h jsteg.h
OBJ = memefs.o blocklayer.o

_GOFILES = main.go reader.go writer.go scan.go huffman.go fdct.go
GOFILES = $(patsubst %,jsteg/%,$(_GOFILES))

memefs: $(OBJ) jsteg.a
	gcc -pthread -o $@ $^ $(CFLAGS) $(PKGFLAGS)

jsteg.a: $(GOFILES)
	go build -buildmode=c-archive -o jsteg.a $(GOFILES)

jsteg.h: jsteg.a
## Recover from the removal of $@
	@if test -f $@; then :; else \
	  rm -f jsteg.a; \
	  $(MAKE) $(AM_MAKEFLAGS) jsteg.a; \
	fi

blocklayer.o: blocklayer.c $(DEPS) queue.c queue_desc.c memedl.c
	$(CC) -c -o $@ $< $(CFLAGS) $(PKGFLAGS)

memedl.o: memedl.c $(DEPS) getinmemory.c urlextract.c url2file.c string_queue.c
	$(CC) -c -o $@ $< $(CFLAGS) $(PKGFLAGS)

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS) $(PKGFLAGS)

.PHONY: clean

clean:
	rm -f *.o jsteg.a jsteg.h memefs
