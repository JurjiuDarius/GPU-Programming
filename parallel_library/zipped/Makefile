# Simple Makefile for User-Level Thread Library
CC = gcc
CFLAGS = -Wall -Wextra -g -D_POSIX_C_SOURCE=200809L

all: test_threads

test_threads: test_threads.c ult.c ult.h
	$(CC) $(CFLAGS) -o test_threads test_threads.c ult.c

clean:
	rm -f test_threads

.PHONY: all clean
