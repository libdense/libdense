CXX ?= g++

ROOT := ../..
# This release package ships prebuilt libraries; the wrapper links against
# them directly. Override DENSE_LIB_DIR / DENSE_INCLUDE_DIR for other layouts.
DENSE_LIB_DIR ?= $(ROOT)/lib/linux-x86_64
DENSE_INCLUDE_DIR ?= $(ROOT)/include/dense
CORE_LIB := $(DENSE_LIB_DIR)/libdense_sim.a
BUILD_DIR := build
TEST_BIN := $(BUILD_DIR)/test_binding
EXAMPLE_BIN := $(BUILD_DIR)/basic_world
BENCH_BIN := $(BUILD_DIR)/wrapper_overhead

CPPFLAGS := -Iinclude -I$(DENSE_INCLUDE_DIR)
BASE_CXXFLAGS := -std=c++20 -Wall -Wextra -Wpedantic -Wconversion -Wshadow
LIBDENSE_BASE_CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wstrict-prototypes -Wmissing-prototypes
CXXFLAGS ?= -O3 $(BASE_CXXFLAGS)
LDFLAGS ?=
LDLIBS := $(CORE_LIB) -lm

.PHONY: all build clean test example benchmark benchmark-native strict

all: build

build:
	rm -f $(TEST_BIN) $(EXAMPLE_BIN)
	$(MAKE) $(TEST_BIN) $(EXAMPLE_BIN)

$(TEST_BIN): tests/test_binding.cpp include/dense_sim.hpp $(CORE_LIB)
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< $(LDLIBS) $(LDFLAGS) -o $@

$(BENCH_BIN): benchmarks/wrapper_overhead.cpp include/dense_sim.hpp $(CORE_LIB)
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< $(LDLIBS) $(LDFLAGS) -o $@

$(EXAMPLE_BIN): examples/basic_world.cpp include/dense_sim.hpp $(CORE_LIB)
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< $(LDLIBS) $(LDFLAGS) -o $@

test:
	rm -f $(TEST_BIN)
	$(MAKE) $(TEST_BIN)
	$(TEST_BIN)
	python3 ../check_abi_coverage.py

example:
	rm -f $(EXAMPLE_BIN)
	$(MAKE) $(EXAMPLE_BIN)
	$(EXAMPLE_BIN)

benchmark:
	rm -f $(BENCH_BIN)
	$(MAKE) $(BENCH_BIN)
	$(BENCH_BIN)

benchmark-native:
	rm -f $(BENCH_BIN)
	$(MAKE) CXXFLAGS="-O3 -march=native -mtune=native -flto -fomit-frame-pointer -fno-semantic-interposition -fno-plt -funroll-loops -DNDEBUG $(BASE_CXXFLAGS)" LDFLAGS="-flto" $(BENCH_BIN)
	$(BENCH_BIN)

strict:
	rm -f $(TEST_BIN)
	$(MAKE) CXX=clang++ CXXFLAGS="-O2 $(BASE_CXXFLAGS) -Werror" $(TEST_BIN)
	$(TEST_BIN)
	python3 ../check_abi_coverage.py

clean:
	rm -rf $(BUILD_DIR)
