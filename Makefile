CC = gcc 
CFLAGS = -Wall -Wextra -g  

all: main.o rawmode.o editor.o
	$(CC) $(CFLAGS) main.o rawmode.o  editor.o -o seed

main.o: main.c rawmode.h
	$(CC) $(CFLAGS) -c main.c -o main.o

rawmode.o: rawmode.c rawmode.h
	$(CC) $(CFLAGS) -c rawmode.c -o rawmode.o

editor.o: editor.c editor.h
	$(CC) $(CFLAGS) -c editor.c -o editor.o

clean:
	rm -f main.o rawmode.o editor.o seed