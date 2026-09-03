# iron86 — C++23 8086 (debug default). gcc/g++ only.
CXX := g++ -Wl,--as-needed
MAKEFLAGS += --no-print-directory

CXXFLAGS_COMMON := -std=gnu++23 -Wall -Wextra -Wpedantic -Wshadow -Wconversion \
	-Iinclude
CXXFLAGS_DEBUG := $(CXXFLAGS_COMMON) -g3 -O0 -fsanitize=address,undefined \
	-fno-omit-frame-pointer
LDFLAGS_DEBUG := -fsanitize=address,undefined

CXXFLAGS ?= $(CXXFLAGS_DEBUG)
LDFLAGS ?= $(LDFLAGS_DEBUG)

.PHONY: all test tests verify clean

all: iron86 tests/run_tests tests/run_pc tests/run_crtc tests/run_pack

SRC := src/cpu.cpp src/ea.cpp src/pc.cpp src/crtc.cpp src/flopfs_pack.cpp
HDR := include/iron86/cpu.h include/iron86/ea.h include/iron86/pc.h include/iron86/hw.h \
	include/iron86/crtc.h include/iron86/flopfs_pack.h

iron86: $(SRC) src/main.cpp $(HDR)
	$(CXX) $(CXXFLAGS) $(SRC) src/main.cpp -o $@ $(LDFLAGS)

tests/run_tests: $(SRC) tests/test_cpu.cpp $(HDR)
	$(CXX) $(CXXFLAGS) src/cpu.cpp src/ea.cpp tests/test_cpu.cpp -o $@ $(LDFLAGS)

tests/run_pc: $(SRC) tests/test_pc.cpp $(HDR)
	$(CXX) $(CXXFLAGS) $(SRC) tests/test_pc.cpp -o $@ $(LDFLAGS)

tests/run_crtc: src/crtc.cpp tests/test_crtc.cpp include/iron86/crtc.h
	$(CXX) $(CXXFLAGS) src/crtc.cpp tests/test_crtc.cpp -o $@ $(LDFLAGS)

tests/run_pack: src/flopfs_pack.cpp tests/test_pack.cpp include/iron86/flopfs_pack.h
	$(CXX) $(CXXFLAGS) src/flopfs_pack.cpp tests/test_pack.cpp -o $@ $(LDFLAGS)

test: tests/run_tests tests/run_pc tests/run_crtc tests/run_pack iron86
	./tests/run_tests
	./tests/run_pc
	./tests/run_crtc
	./tests/run_pack
	./iron86 -h | grep -q Usage
	./iron86 -v | grep -q 'iron86 0.13'
	./iron86 --no-audio -v | grep -q 'iron86 0.13'
	./iron86 -h | grep -q -- --exec-map
	./iron86 >/tmp/iron86-usage.txt
	grep -q Usage /tmp/iron86-usage.txt
	grep -q -- --no-fast-post /tmp/iron86-usage.txt
	grep -q -- --no-audio /tmp/iron86-usage.txt
	grep -q -- --floppy-b /tmp/iron86-usage.txt
	./iron86 -h | grep -q -- --floppy-b
	./iron86 -h | grep -q -- --floppy-a

tests: test

.PHONY: floppy
floppy: iron86
	./iron86 --floppy ../floppyos/build/floppyos.img --keys $$'FCBTEST\r'

verify: test
	@echo "iron86: no CBMC model yet; opcode CPU is Catch-free snippet tests"

clean:
	rm -f iron86 tests/run_tests tests/run_pc tests/run_crtc tests/run_pack
