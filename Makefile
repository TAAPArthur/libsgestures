DEBUGGING_FLAGS := -std=c11 -g -rdynamic -O0 -Werror -Wall -Wextra -Wno-missing-field-initializers -Wno-sign-compare -Wno-parentheses -Wno-missing-braces
RELEASE_FLAGS ?= -std=c11 -O3 -DNDEBUG -Werror -Wall -Wextra -Wno-missing-field-initializers -Wno-parentheses -Wno-missing-braces
CFLAGS ?= $(RELEASE_FLAGS)
LDFLAGS := -lm
SRC := gesture-event.c gestures-reader.c gestures-recorder.c
pkgname := sgestures


all: libsgestures.a sample-gesture-reader sgestures-libinput-writer

install-headers:
	install -m 0744 -Dt "$(DESTDIR)/usr/include/$(pkgname)/" *.h

install: install-headers sgestures-libinput-writer libsgestures.a sgestures.sh
	install -m 0744 -Dt "$(DESTDIR)/usr/lib/" libsgestures.a
	install -m 0755 -Dt "$(DESTDIR)/usr/bin/" sgestures-libinput-writer
	install -m 0755 sgestures.sh "$(DESTDIR)/usr/bin/sgestures"
	install -m 0755 -Dt "$(DESTDIR)/usr/share/sgestures/" sample-gesture-reader.c

uninstall:
	rm -f "$(DESTDIR)/usr/lib/libsgestures.a"
	rm -f "$(DESTDIR)/usr/bin/sgestures-libinput-writer"
	rm -rdf "$(DESTDIR)/usr/include/$(pkgname)"

libsgestures.a: $(SRC:.c=.o)
	ar rcs $@ $^

test: gesture-test  libinput-gesture-test
	./gesture-test
	./libinput-gesture-test

sgestures.sh: sgestures.sh.template
	sed "s/CFLAGS:-__place__holder/CFLAGS:-'${CFLAGS}'/g; s/LDFLAGS:-__place__holder/LDFLAGS:-'${LDFLAGS}'/g" $< > $@

sgestures-libinput-writer: gestures-libinput-writer.o
	$(CC) $(CFLAGS) $^ -o $@ -ludev -linput

gesture-test: CFLAGS := $(DEBUGGING_FLAGS)
gesture-test: $(SRC:.c=.o) gestures_unit.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) -lscutest

libinput-gesture-test: CFLAGS := $(DEBUGGING_FLAGS)
libinput-gesture-test: $(SRC:.c=.o) libinput_gestures_unit.o  gestures-libinput-writer.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) -lscutest -ludev -linput


sample-gesture-reader: CFLAGS := $(DEBUGGING_FLAGS)
sample-gesture-reader: sample-gesture-reader.o $(SRC:.c=.o)
	$(CC) $(CFLAGS) $^ -o $@ -lm

debug: CFLAGS := $(DEBUGGING_FLAGS)
debug: sgestures-libinput-writer sample-gesture-reader
	./sgestures-libinput-writer | ./sample-gesture-reader

clean:
	rm -f *.{o,a} *-test

.PHONY: clean install uninstall install-headers

.DELETE_ON_ERROR:
