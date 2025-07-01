CC = gcc 
CFLAGS = -Wall -Wextra -g  

all: main.o rawmode.o
	$(CC) $(CFLAGS) main.o rawmode.o -o seed

main.o: main.c rawmode.h
	$(CC) $(CFLAGS) -c main.c -o main.o

rawmode.o: rawmode.c rawmode.h
	$(CC) $(CFLAGS) -c rawmode.c -o rawmode.o

clean:
	rm -f main.o rawmode.o seed