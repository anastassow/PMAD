CC = gcc
CFLAGS = -g -Iinclude

SRC = src/PMAD.c src/incPMAD.c src/MemoryPool.c src/BlockHeader.c
OBJ = PMAD.o incPMAD.o MemoryPool.o BlockHeader.o main.o 

all: main

main: $(OBJ)
	$(CC) $(OBJ) -o main

main.o: main.c
	$(CC) $(CFLAGS) -c main.c -o main.o

PMAD.o: src/PMAD.c
	$(CC) $(CFLAGS) -c src/PMAD.c -o PMAD.o

incPMAD.o: src/incPMAD.c
	$(CC) $(CFLAGS) -c src/incPMAD.c -o incPMAD.o

MemoryPool.o: src/MemoryPool.c
	$(CC) $(CFLAGS) -c src/MemoryPool.c -o MemoryPool.o

BlockHeader.o: src/BlockHeader.c
	$(CC) $(CFLAGS) -c src/BlockHeader.c -o BlockHeader.o

clean:
	rm -f $(OBJ) main
