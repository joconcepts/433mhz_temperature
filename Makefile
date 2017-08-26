CC=gcc

CFLAGS=-std=gnu99
LIBRARIES=-lwiringPi -lpthread

all:  weathermon

weathermon: weathermon.h weathermon.c
	$(CC) $(CFLAGS) -o weathermon weathermon.c $(LIBRARIES)
	chmod +x weathermon

clean: 
	rm weathermon
