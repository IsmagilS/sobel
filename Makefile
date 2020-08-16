all: main.c
	gcc -o main -std=c99 -lpthread main.c