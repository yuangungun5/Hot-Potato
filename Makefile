CFLAGS= -std=gnu99 -pedantic -ggdb3

all: player ringmaster

player: player.c potato.h
	gcc $(CFLAGS) -o player player.c

ringmaster: ringmaster.c potato.h
	gcc $(CFLAGS) -o ringmaster ringmaster.c

clean:
	rm -f *.o *~ player ringmaster
