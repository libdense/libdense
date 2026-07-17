CXX ?= g++

ROOT := ../..
INCLUDE_DIR ?= $(ROOT)/include/dense
LIB_DIR ?= $(ROOT)/lib/linux-x86_64
CORE_LIB ?= $(LIB_DIR)/libdense_sim.a
BUILD_DIR := build
TEST_BIN := $(BUILD_DIR)/test_binding
EXAMPLE_BIN := $(BUILD_DIR)/basic_world
BENCH_BIN := $(BUILD_DIR)/wrapper_overhead

CPPFLAGS := -Iinclude/dense -I$(INCLUDE_DIR)
BASE_CXXFLAGS := -std=c++20 -Wall -Wextra -Wpedantic -Wconversion -Wshadow
CXXFLAGS ?= -O3 $(BASE_CXXFLAGS)
LDFLAGS ?=
LDLIBS := $(CORE_LIB) -lm

.PHONY: all build clean test example benchmark strict

all: build

build: $(TEST_BIN) $(EXAMPLE_BIN)

$(CORE_LIB):
	@printf "Missing %s\n" "$@" >&2
	@printf "Set CORE_LIB or LIB_DIR to the packaged libdense_sim.a.\n" >&2
	@exit 1

$(TEST_BIN): tests/test_binding.cpp include/dense/dense_sim.hpp $(CORE_LIB)
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< $(LDLIBS) $(LDFLAGS) -o $@

$(BENCH_BIN): benchmarks/wrapper_overhead.cpp include/dense/dense_sim.hpp $(CORE_LIB)
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< $(LDLIBS) $(LDFLAGS) -o $@

$(EXAMPLE_BIN): examples/basic_world.cpp include/dense/dense_sim.hpp $(CORE_LIB)
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< $(LDLIBS) $(LDFLAGS) -o $@

test: $(TEST_BIN)
	$(TEST_BIN)
	python3 ../check_abi_coverage.py

example: $(EXAMPLE_BIN)
	$(EXAMPLE_BIN)

benchmark: $(BENCH_BIN)
	$(BENCH_BIN)

strict:
	rm -f $(TEST_BIN)
	$(MAKE) CXX=clang++ CXXFLAGS="-O2 $(BASE_CXXFLAGS) -Werror" $(TEST_BIN)
	$(TEST_BIN)
	python3 ../check_abi_coverage.py

clean:
	rm -rf $(BUILD_DIR)
