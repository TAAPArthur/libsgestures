CC := gcc
DEBUGGING_FLAGS := -std=c11 -g -rdynamic -O0 -Werror -Wall -Wextra -Wno-missing-field-initializers -Wno-sign-compare -Wno-parentheses -Wno-missing-braces
RELEASE_FLAGS ?= -std=c11 -O3 -DNDEBUG -Werror -Wall -Wextra -Wno-missing-field-initializers -Wno-parentheses -Wno-missing-braces
CFLAGS ?= $(RELEASE_FLAGS)
LDFLAGS := -lm
SRC := gesture-event.c gestures-reader.c gestures-recorder.c
pkgname := sgestures


all: sgestures-libinput-writer libsgestures.a

install-headers:
	install -m 0744 -Dt "$(DESTDIR)/usr/include/$(pkgname)/" *.h

install: install-headers sgestures-libinput-writer libsgestures.a
	install -m 0744 -Dt "$(DESTDIR)/usr/lib/" libsgestures.a
	install -m 0755 -Dt "$(DESTDIR)/usr/bin/" sgestures-libinput-writer

uninstall:
	rm -f "$(DESTDIR)/usr/lib/libsgestures.a"
	rm -f "$(DESTDIR)/usr/bin/sgestures-libinput-writer"
	rm -rdf "$(DESTDIR)/usr/include/$(pkgname)"

libsgestures.a: $(SRC:.c=.o)
	ar rcs $@ $^

test: gesture-test sgestures-libinput-writer libsgestures.a
	./gesture-test

sgestures-libinput-writer: gestures-libinput-writer.o
	$(CC) $(CFLAGS) $^ -o $@ -ludev -linput

sample-gesture-reader: sample-gesture-reader.o
	$(CC) $(CXXFLAGS) $^ -o $@ -lsgestures -lm

gesture-test: CFLAGS := $(DEBUGGING_FLAGS)
gesture-test: $(SRC:.c=.o) gestures_unit.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) -lscutest

clean:
	rm -f *.{o,a} gesture-test

.PHONY: clean install uninstall install-headers

.DELETE_ON_ERROR:
