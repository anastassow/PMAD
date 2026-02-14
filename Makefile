CC = gcc
CFLAGS = -g -Iinclude

SRC = src/PMAD.c src/MemoryPool.c
OBJ = PMAD.o MemoryPool.o main.o

all: main

main: $(OBJ)
	$(CC) $(OBJ) -o main

main.o: main.c
	$(CC) $(CFLAGS) -c main.c -o main.o

PMAD.o: src/PMAD.c
	$(CC) $(CFLAGS) -c src/PMAD.c -o PMAD.o

MemoryPool.o: src/MemoryPool.c
	$(CC) $(CFLAGS) -c src/MemoryPool.c -o MemoryPool.o

clean:
	rm -f $(OBJ) main
