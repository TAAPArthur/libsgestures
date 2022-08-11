DEBUGGING_FLAGS := -std=c99 -g -rdynamic -O0 -Werror -Wall -Wextra -Wno-missing-field-initializers -Wno-sign-compare -Wno-parentheses -Wno-missing-braces -DDEBUG
RELEASE_FLAGS = -std=c99 -O3 -DNDEBUG -Werror -Wall -Wextra -Wno-missing-field-initializers -Wno-parentheses -Wno-missing-braces
CFLAGS_0 = $(RELEASE_FLAGS)
CFLAGS_1 = $(DEBUGGING_FLAGS)
DEBUG = 0
CFLAGS ?= $(CFLAGS_$(DEBUG))
LDFLAGS := -lm
SRC := gesture-event.c gestures-reader.c gestures-recorder.c
pkgname := sgestures


all: libsgestures.a sgestures-libinput-writer sgestures

install-headers:
	install -m 0744 -Dt "$(DESTDIR)/usr/include/$(pkgname)/" *.h

install: install-headers sgestures-libinput-writer libsgestures.a sgestures.sh sgestures
	install -m 0744 -Dt "$(DESTDIR)/usr/lib/" libsgestures.a
	install -m 0755 -Dt "$(DESTDIR)/usr/bin/" sgestures-libinput-writer
	install -m 0755 sgestures.sh "$(DESTDIR)/usr/bin/sgestures"
	install -m 0755 -Dt "$(DESTDIR)/usr/share/sgestures/" sample-gesture-reader.c
	install -m 0755 -Dt "$(DESTDIR)/usr/libexec/" sgestures

uninstall:
	rm -f "$(DESTDIR)/usr/lib/libsgestures.a"
	rm -f "$(DESTDIR)/usr/bin/sgestures-libinput-writer"
	rm -rdf "$(DESTDIR)/usr/include/$(pkgname)"
	rm "$(DESTDIR)/usr/libexec/$(pkgname)"

libsgestures.a: $(SRC:.c=.o)
	ar rcs $@ $^

test: gesture-test  libinput-gesture-test
	./gesture-test
	./libinput-gesture-test

config.c: sample-gesture-reader.c
	cp $^ $@

sgestures-libinput-writer: gestures-libinput-writer.o
	$(CC) $(CFLAGS) $^ -o $@ -linput -lm -ludev -levdev -lmtdev

gesture-test: CFLAGS := $(DEBUGGING_FLAGS)
gesture-test: tests/gestures_unit.o $(SRC:.c=.o)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

libinput-gesture-test: CFLAGS := $(DEBUGGING_FLAGS)
libinput-gesture-test: $(SRC:.c=.o) tests/libinput_gestures_unit.o  gestures-libinput-writer.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) -ludev -linput

sgestures: config.o $(SRC:.c=.o)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

sample-gesture-reader: sample-gesture-reader.o $(SRC:.c=.o)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

debug: sgestures-libinput-writer sample-gesture-reader
	./sgestures-libinput-writer | ./sample-gesture-reader $(MASK)

clean:
	rm -f *.o tests/*.o *.a *-test sgestures sample-gesture-reader sgestures-libinput-writer

.PHONY: clean install uninstall install-headers

.DELETE_ON_ERROR:
