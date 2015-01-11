
CFLAGS=-g -Wall -Werror -std=c99 -pedantic-errors
SRC=src/*.c
HEAD=src/*.h

all: bin/bchip

bin/bchip: $(SRC) $(HEAD)
	mkdir -p bin
	gcc -lSDL2 $(CFLAGS) -o $@ $(SRC)

clean:
	rm -Rf bin/*
	rm -f tmp.c8
