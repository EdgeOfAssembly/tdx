# iron86 — C++23 8086 (debug default). gcc/g++ only.
CXX := g++ -Wl,--as-needed
MAKEFLAGS += --no-print-directory

CXXFLAGS_COMMON := -std=gnu++23 -Wall -Wextra -Wpedantic -Wshadow -Wconversion \
	-Iinclude
CXXFLAGS_OPTIMIZED := -O3 -march=x86-64 -mtune=generic -fno-omit-frame-pointer
CXXFLAGS_DEBUG := $(CXXFLAGS_COMMON) -g3 -O0 -fsanitize=address,undefined \
	-fno-omit-frame-pointer
LDFLAGS_DEBUG := -fsanitize=address,undefined
CXXFLAGS_RELEASE := $(CXXFLAGS_COMMON) -DNDEBUG $(CXXFLAGS_OPTIMIZED)
LDFLAGS_RELEASE :=
CXXFLAGS_PROFILE := $(CXXFLAGS_COMMON) -DNDEBUG $(CXXFLAGS_OPTIMIZED) -g -pg -fno-inline
LDFLAGS_PROFILE := -pg

CXXFLAGS ?= $(CXXFLAGS_DEBUG)
LDFLAGS ?= $(LDFLAGS_DEBUG)

.PHONY: all test tests verify clean release profile

BUILD_FLAGS := -s V=0 -j$(shell nproc 2>/dev/null || echo 1)

all: iron86 tests/run_tests tests/run_pc tests/run_crtc tests/run_pack tests/run_config \
	tests/run_hdc

SRC := src/cpu.cpp src/ea.cpp src/pc.cpp src/crtc.cpp src/flopfs_pack.cpp src/config.cpp \
	src/hdc.cpp
HDR := include/iron86/cpu.h include/iron86/ea.h include/iron86/pc.h include/iron86/hw.h \
	include/iron86/crtc.h include/iron86/flopfs_pack.h include/iron86/config.h \
	include/iron86/chs_lba.h include/iron86/hdc.h

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

tests/run_config: src/config.cpp tests/test_config.cpp include/iron86/config.h include/iron86/cpu.h \
	include/iron86/hdc.h
	$(CXX) $(CXXFLAGS) src/config.cpp tests/test_config.cpp -o $@ $(LDFLAGS)

tests/run_hdc: $(SRC) tests/test_hdc.cpp $(HDR)
	$(CXX) $(CXXFLAGS) $(SRC) tests/test_hdc.cpp -o $@ $(LDFLAGS)

test: tests/run_tests tests/run_pc tests/run_crtc tests/run_pack tests/run_config tests/run_hdc iron86
	./tests/run_tests
	./tests/run_pc
	./tests/run_crtc
	./tests/run_pack
	./tests/run_config
	./tests/run_hdc
	./iron86 -h | grep -q Usage
	./iron86 -v | grep -q 'iron86 0.17'
	./iron86 --no-audio -v | grep -q 'iron86 0.17'
	./iron86 -h | grep -q -- --exec-map
	./iron86 -h | grep -q -- --prefetch-bytes
	./iron86 -h | grep -q -- --until-prompt
	./iron86 -h | grep -q -- --config
	./iron86 -h | grep -q -- --preset
	./iron86 -h | grep -q -- --hdc
	./iron86 -h | grep -q -- --hdc-rom
	./iron86 -h | grep -q 160K
	./iron86 -h | grep -q 1.2M
	./iron86 >/tmp/iron86-usage.txt
	./iron86 -h >/tmp/iron86-h.txt
	diff -q /tmp/iron86-usage.txt /tmp/iron86-h.txt
	grep -q Usage /tmp/iron86-usage.txt
	grep -q -- --no-fast-post /tmp/iron86-usage.txt
	grep -q -- --no-audio /tmp/iron86-usage.txt
	grep -q -- --floppy-b /tmp/iron86-usage.txt
	./iron86 -h | grep -q -- --floppy-b
	./iron86 -h | grep -q -- --floppy-a
	./iron86 --preset ibm-at >/tmp/iron86-at.out 2>/tmp/iron86-at.err; test $$? -eq 2
	grep -q 'not yet' /tmp/iron86-at.err
	./iron86 --preset 8286 >/tmp/iron86-8286.out 2>/tmp/iron86-8286.err; test $$? -eq 2
	grep -q 'not yet' /tmp/iron86-8286.err

tests: test

.PHONY: floppy
floppy: iron86
	./iron86 --floppy ../floppyos/build/floppyos.img --keys $$'FCBTEST\r'

CBMC ?= $(HOME)/.local/bin/cbmc
verify: test
	@if [ -x "$(CBMC)" ]; then \
		echo "iron86: CBMC $(CBMC) chs_lba"; \
		$(CBMC) formal/verify_chs_lba.c -Iinclude --bounds-check --pointer-check \
			--unwind 16 --unwinding-assertions; \
		echo "iron86: CBMC $(CBMC) hdc_lba"; \
		$(CBMC) formal/verify_hdc_lba.c -Iinclude --bounds-check --pointer-check \
			--unwind 8 --unwinding-assertions; \
	else \
		echo "iron86: CBMC not at $(CBMC); verify = test only"; \
	fi

release:
	$(MAKE) $(BUILD_FLAGS) clean CXXFLAGS="$(CXXFLAGS_RELEASE)" \
		LDFLAGS="$(LDFLAGS_RELEASE)" all

profile:
	$(MAKE) $(BUILD_FLAGS) clean CXXFLAGS="$(CXXFLAGS_PROFILE)" \
		LDFLAGS="$(LDFLAGS_PROFILE)" all
	@echo "Profile binary: ./iron86  (run, then gprof ./iron86 gmon.out)"

.PHONY: bench-boot
bench-boot:
	$(MAKE) $(BUILD_FLAGS) CXXFLAGS="$(CXXFLAGS_RELEASE)" \
		LDFLAGS="$(LDFLAGS_RELEASE)" iron86
	@echo "=== bench-boot 5150+FloppyOS until A> ==="
	@/usr/bin/time -f 'real %e sec user %U steps-see-stderr' \
		./iron86 --bios /mnt/TurboDebugger/Py86/ROM/IBM/PC/5150/BIOS_IBM5150_24APR81_5700051_U33.BIN \
		--floppy-a ../floppyos/build/floppyos.img --until-prompt --no-audio

clean:
	rm -f iron86 tests/run_tests tests/run_pc tests/run_crtc tests/run_pack \
		tests/run_config tests/run_hdc gmon.out
