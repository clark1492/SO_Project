#########################################################################################################################
# Makefile.																												#
#																														#																						#
#########################################################################################################################

CFLAGS=-g -O0 -Wall -std=c89 -pedantic -D_GNU_SOURCE #-Werror
OPTIONS=-D "LOG_TO_CONSOLE"

all: master ship harbour

master: master.o logger.o common.o shm_utils.o
	gcc master.o logger.o common.o shm_utils.o -o master -lm

ship: ship.o logger.o common.o shm_utils.o
	gcc ship.o logger.o common.o shm_utils.o -o ship -lm

harbour: harbour.o logger.o common.o shm_utils.o
	gcc harbour.o logger.o common.o shm_utils.o -o harbour -lm

master.o: master.c
	gcc -c $(CFLAGS) $(OPTIONS) master.c

child.o: child.c
	gcc -c $(CFLAGS) child.c

ship.o: ship.c
	gcc -c $(CFLAGS) ship.c

harbour.o: harbour.c
	gcc -c $(CFLAGS) harbour.c

shm_utils.o: shm_utils.h shm_utils.c
	gcc -c $(CFLAGS) shm_utils.c

logger.o: logger.c logger.h common.o
	gcc -c $(CFLAGS) logger.c

common.o: common.c common.h
	gcc -c $(CFLAGS) common.c



clean:
	rm -f *.o child master
