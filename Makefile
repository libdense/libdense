PREFIX ?= /usr/local
DESTDIR ?=
LIBDIR ?= lib
PYTHON ?= python3

.PHONY: all help install uninstall verify checksums python-test cpp-test rust-test binding-test clean

all: help

help:
	@printf '%s\n' \
		"Dense public binary release" \
		"" \
		"  make install PREFIX=/usr/local" \
		"  make uninstall PREFIX=/usr/local" \
		"  make verify" \
		"  make binding-test" \
		"  make checksums"

install:
	PREFIX="$(PREFIX)" DESTDIR="$(DESTDIR)" LIBDIR="$(LIBDIR)" ./install.sh

uninstall:
	PREFIX="$(PREFIX)" DESTDIR="$(DESTDIR)" LIBDIR="$(LIBDIR)" ./uninstall.sh

verify:
	./verify-release.sh

checksums:
	./tools/generate-checksums.sh

python-test:
	$(MAKE) -C bindings/python PYTHON="$(PYTHON)" test

cpp-test:
	$(MAKE) -C bindings/cpp test

rust-test:
	$(MAKE) -C bindings/rust test

binding-test: python-test cpp-test rust-test

clean:
	$(MAKE) -C bindings/python clean
	$(MAKE) -C bindings/cpp clean
	$(MAKE) -C bindings/rust clean
	rm -rf stage package
