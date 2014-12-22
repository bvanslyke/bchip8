CFLAGS=-g -Wall -Werror -std=c99 -pedantic-errors

all: bin/bchip

bin/bchip: src/bchip.c
	mkdir -p bin
	gcc -lSDL2 $(CFLAGS) -o $@ $^

clean:
	rm -Rf bin/*
	rm -f tmp.c8
