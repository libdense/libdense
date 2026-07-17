CARGO ?= cargo
ROOT := ../..
LIB_DIR ?= $(ROOT)/lib/linux-x86_64
MANIFEST := Cargo.toml

.PHONY: all build clean test example fmt-check clippy

all: build

build:
	DENSE_SIM_LIB_DIR="$(abspath $(LIB_DIR))" $(CARGO) build --manifest-path $(MANIFEST)

test:
	DENSE_SIM_LIB_DIR="$(abspath $(LIB_DIR))" $(CARGO) test --manifest-path $(MANIFEST)

example:
	DENSE_SIM_LIB_DIR="$(abspath $(LIB_DIR))" $(CARGO) run --manifest-path $(MANIFEST) --example basic_world

fmt-check:
	$(CARGO) fmt --manifest-path $(MANIFEST) -- --check

clippy:
	DENSE_SIM_LIB_DIR="$(abspath $(LIB_DIR))" $(CARGO) clippy --manifest-path $(MANIFEST) --all-targets -- -D warnings

clean:
	rm -rf target
