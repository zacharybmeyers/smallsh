setup:
	gcc -std=c99 -g -Wall -o smallsh smallsh.c
clean:
	rm -f smallsh
valgrind:
	valgrind --leak-check=yes --track-origins=yes ./smallsh