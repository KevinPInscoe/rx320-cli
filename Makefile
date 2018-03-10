#!/usr/bin/make
#
# Makefile for rx320cli command line program.
#
# A. Maitland Bottoms <aa4hs@amrad.org>
# December 9, 2000
#
# Kevin Inscoe <kevin@inscoe.org>
# March 7, 2009
#
# Renamed the program to rx320cli to avoid confusion with
# Hector Peraza's rx320, which is based on the xclass libs.
#
# Also took over maintenance of the source code seeing
# no one else who is.

DESTDIR=/usr/local

all: rx320cli rx320cli.1

rx320cli:	src/rx320cli.c src/rx320cli.h
	gcc -Wall -o bin/rx320cli src/rx320cli.c

rx320cli.1: man/rx320cli.sgml
	#docbook2man  man/rx320cli.sgml > man/rx320cli.1
	docbook2man  man/rx320cli.sgml
	mv RX320.1 man/rx320cli.1

install:
	install -d $(DESTDIR)/bin
	install bin/rx320cli $(DESTDIR)/bin/rx320cli
	install man/rx320cli.1 $(DESTDIR)/man/man1/rx320cli.1

clean:
	- rm bin/rx320cli rx320cli.o bin/rx320cli.o man/rx320cli.1 RX320.1
	- rm manpage.links manpage.refs
