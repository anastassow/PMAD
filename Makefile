all:
	gcc -g -c main.c -Iinclude -o main.o
	gcc -g -c PMAD.c -Iinclude/structures/ -o PMAD.o
	gcc main.o PMAD.o -o main

clean:
	rm -f main.o PMAD.o main
