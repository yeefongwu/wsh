# Variables
CC = gcc  # Or you can change to clang if you prefer
CFLAGS = -Wall -Werror -pedantic -g -std=gnu18
LOGIN = yeefong
SUBMITPATH = /home/cs537-1/handin/yeefong/P3

# Targets
.PHONY: all
all: wsh

wsh: wsh.c wsh.h
	$(CC) $(CFLAGS) -o $@ $<

.PHONY: run
run: wsh
	./wsh

.PHONY: pack
pack: 
	tar -cvzf $(LOGIN).tar.gz wsh.c wsh.h Makefile README.md terminal_test.log

.PHONY: submit
submit: pack
	cp $(LOGIN).tar.gz $(SUBMITPATH)
