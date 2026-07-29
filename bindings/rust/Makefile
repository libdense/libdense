CARGO ?= cargo
MANIFEST := Cargo.toml

.PHONY: all build clean test example fmt-check clippy

all: build

build:
	$(CARGO) build --manifest-path $(MANIFEST)

test:
	$(CARGO) test --manifest-path $(MANIFEST)

example:
	$(CARGO) run --manifest-path $(MANIFEST) --example basic_world

fmt-check:
	$(CARGO) fmt --manifest-path $(MANIFEST) -- --check

clippy:
	$(CARGO) clippy --manifest-path $(MANIFEST) --all-targets -- -D warnings

clean:
	rm -rf target
