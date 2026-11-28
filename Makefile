CC ?= cc
MPICC ?= mpicc
CFLAGS ?= -O3 -std=c11 -Wall -Wextra -Wpedantic
LDFLAGS ?= -lm

.PHONY: all clean check
all: build/sequential build/parallel

build:
	mkdir -p build

build/sequential: src/sequential.c src/solver.h | build
	$(CC) $(CFLAGS) -o $@ src/sequential.c $(LDFLAGS)

build/parallel: src/parallel.c src/solver.h | build
	$(MPICC) $(CFLAGS) -o $@ src/parallel.c $(LDFLAGS)

check: all
	python3 scripts/check.py

clean:
	rm -rf build
