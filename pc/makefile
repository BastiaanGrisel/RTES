# variables: compiler to use and flags for it
CC = gcc
CFLAGS = -lncurses -I../include

pc.o: pc.c
	$(CC) $^ -o $@ $(CFLAGS)
# $^ means "everything right off the :"
# $@ means "everything left of the :"
# so actually the rule says:
# 		gcc keysandjs.c -o kj -lncurses

clean:
	rm pc.o -v # remove files 
