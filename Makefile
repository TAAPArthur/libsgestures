CXX := g++
CXXFLAGS := -std=c++17 -lstdc++
DEBUGGING_FLAGS := -g -rdynamic -O0 -D_GLIBCXX_DEBUG
RELEASE_FLAGS ?= -O3 -DNDEBUG -Werror -Wall -Wextra -Wno-missing-field-initializers -Wno-parentheses
CPPFLAGS ?= $(RELEASE_FLAGS)
SRC := gestures.cpp gestures-reader.cpp gestures-recorder.cpp
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

libsgestures.a: $(SRC:.cpp=.o)
	ar rcs $@ $^

test: gesture-test
	./gesture-test

sgestures-libinput-writer: gestures-libinput-writer.o
	$(CXX) $(CXXFLAGS) $^ -o $@ -ludev -linput

sample-gesture-reader: CPPFLAGS := $(DEBUGGING_FLAGS)
sample-gesture-reader: sample-gesture-reader.o $(SRC:.cpp=.o)
	$(CXX) $(CXXFLAGS) $^ -o $@ -ludev -linput -lpthread -lm

gesture-test: CPPFLAGS := $(DEBUGGING_FLAGS)
gesture-test: tester.o $(SRC:.cpp=.o) gestures_unit.o
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	rm -f *.{o,a} gesture-test

.PHONY: clean install uninstall install-headers

.DELETE_ON_ERROR:
