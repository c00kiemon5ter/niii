# Makefile for niii - see LICENSE for license and copyright information

VERSION = cookies-git
NAME    = niii

PREFIX ?= /usr/local
BINDIR ?= ${PREFIX}/bin
MANPREFIX = ${PREFIX}/share/man

INCS = -I. -I/usr/include
LIBS = -L/usr/lib -lc -lncursesw

CFLAGS = -std=c99 -pedantic -Wall -Wextra -Os ${INCS} -DVERSION=\"$(VERSION)\" \
		 -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=500 # required by getline and realpath
LDFLAGS  = -s ${LIBS}

CC 	 = cc
EXEC = ${NAME}

SRC = ${NAME}.c
OBJ = ${SRC:.c=.o}

all: options ${NAME}

options:
	@echo ${NAME} build options:
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "LDFLAGS  = ${LDFLAGS}"
	@echo "CC       = ${CC}"

.c.o:
	@echo CC $<
	@${CC} -c ${CFLAGS} $<

.o:
	@echo CC -o $@
	@${CC} -o $@ $< ${LDFLAGS}

clean:
	@echo cleaning
	@rm -fv ${NAME} ${OBJ} ${NAME}-${VERSION}.tar.gz

install: all
	@echo installing executable file to ${DESTDIR}${PREFIX}/bin
	@install -Dm755 ${NAME} ${DESTDIR}${PREFIX}/bin/${NAME}
	#@echo installing manual page to ${DESTDIR}${MANPREFIX}/man.1
	#@install -Dm644 ${NAME}.1 ${DESTDIR}${MANPREFIX}/man1/${NAME}.1

uninstall:
	@echo removing executable file from ${DESTDIR}${PREFIX}/bin
	@rm -f ${DESTDIR}${PREFIX}/bin/${NAME}
	#@echo removing manual page from ${DESTDIR}${MANPREFIX}/man1
	#@rm -f ${DESTDIR}${MANPREFIX}/man1/${NAME}.1

.PHONY: all options clean install uninstall
