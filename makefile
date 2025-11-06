# Makefile for Banking Management System

CC = gcc
# -pthread is required for linking the POSIX threads library
CFLAGS = -g -Wall -pthread

# This line ensures init_db is part of the build
BINS = server client init_db

all: $(BINS)

server: server.c common.h
	$(CC) $(CFLAGS) -o server server.c

client: client.c common.h
	$(CC) $(CFLAGS) -o client client.c

# This is the rule to build init_db
init_db: init_db.c common.h
	$(CC) $(CFLAGS) -o init_db init_db.c

clean:
	# This one command forcefully removes all executables, .o files, and .dat files
	rm -f server client init_db *.o db_*.dat

.PHONY: all clean