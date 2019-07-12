CFLAGS =  -Wall -pedantic -Isrc

TARGETS= libfind.o libfind.a

libfind.o: find.c find.h
	$(CC) -c -fPIC ${CFLAGS} -o $@ $<

libfind.a: find.o
	ar rcs $@ $^

all: $(TARGETS)
