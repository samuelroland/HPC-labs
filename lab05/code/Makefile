
CFLAGS=-std=c17 -O2 -march=native
CFLAGS+=-g -Wall -Wextra -Wconversion -Wformat


all: build/ build/create-sample build/analyze

build/:
	mkdir -p build/

build/create-sample: create-sample.c
	$(CC) $(CFLAGS) $^ -lm -o $@

build/analyze: main.c
	$(CC) $(CFLAGS) $^ -lm -o $@

.PHONY: clean
clean:
	rm -r build/
