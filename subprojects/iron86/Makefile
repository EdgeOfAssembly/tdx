# iron86 — C++23 8086 (debug default). gcc/g++ only.
# Release (`make release`) adds -DNDEBUG so #ifndef NDEBUG traces compile out.
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

SDL_CFLAGS := $(shell pkg-config --cflags sdl2 2>/dev/null)
SDL_LIBS := $(shell pkg-config --libs sdl2 2>/dev/null)

.PHONY: all test tests verify clean release profile

BUILD_FLAGS := -s V=0 -j$(shell nproc 2>/dev/null || echo 1)

all: iron86 tests/run_tests tests/run_pc tests/run_crtc tests/run_pack tests/run_config \
	tests/run_hdc tests/run_log tests/run_fpu

SRC := src/cpu.cpp src/ea.cpp src/pc.cpp src/crtc.cpp src/flopfs_pack.cpp src/config.cpp \
	src/hdc.cpp src/log.cpp src/fpu.cpp
HDR := include/iron86/cpu.h include/iron86/ea.h include/iron86/pc.h include/iron86/hw.h \
	include/iron86/crtc.h include/iron86/flopfs_pack.h include/iron86/config.h \
	include/iron86/chs_lba.h include/iron86/hdc.h include/iron86/log.h \
	include/iron86/fpu.h

iron86: $(SRC) src/main.cpp src/display_sdl.cpp include/iron86/display_sdl.h $(HDR)
	$(CXX) $(CXXFLAGS) $(SDL_CFLAGS) $(SRC) src/main.cpp src/display_sdl.cpp -o $@ $(LDFLAGS) $(SDL_LIBS)

tests/run_tests: $(SRC) tests/test_cpu.cpp $(HDR)
	$(CXX) $(CXXFLAGS) src/cpu.cpp src/ea.cpp src/log.cpp src/fpu.cpp tests/test_cpu.cpp -o $@ $(LDFLAGS)

tests/run_fpu: src/fpu.cpp src/cpu.cpp src/ea.cpp src/log.cpp tests/test_fpu.cpp $(HDR)
	$(CXX) $(CXXFLAGS) src/fpu.cpp src/cpu.cpp src/ea.cpp src/log.cpp tests/test_fpu.cpp -o $@ $(LDFLAGS)

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

tests/run_log: src/log.cpp tests/test_log.cpp include/iron86/log.h
	$(CXX) $(CXXFLAGS) src/log.cpp tests/test_log.cpp -o $@ $(LDFLAGS)

test: tests/run_tests tests/run_pc tests/run_crtc tests/run_pack tests/run_config tests/run_hdc \
	tests/run_log tests/run_fpu iron86
	./tests/run_tests
	./tests/run_pc
	./tests/run_crtc
	./tests/run_pack
	./tests/run_config
	./tests/run_hdc
	./tests/run_log
	./tests/run_fpu
	./iron86 -h | grep -q Usage
	./iron86 -v | grep -q 'iron86 0.24'
	./iron86 --no-audio -v | grep -q 'iron86 0.24'
	./iron86 -h | grep -q -- --headless
	./iron86 -h | grep -q -- --scale
	./iron86 -h | grep -q -- --exec-map
	./iron86 -h | grep -q -- --prefetch-bytes
	./iron86 -h | grep -q -- --until-prompt
	./iron86 -h | grep -q -- --config
	./iron86 -h | grep -q -- --preset
	./iron86 -h | grep -q -- --hdc
	./iron86 -h | grep -q -- --hdc-rom
	./iron86 -h | grep -q -- --log
	./iron86 -h | grep -q -- --lock-mem
	./iron86 -h | grep -q 160K
	./iron86 -h | grep -q 1.2M
	./iron86 >/tmp/iron86-usage.txt
	./iron86 -h >/tmp/iron86-h.txt
	diff -q /tmp/iron86-usage.txt /tmp/iron86-h.txt
	grep -q Usage /tmp/iron86-usage.txt
	grep -q -- --no-fast-post /tmp/iron86-usage.txt
	grep -q -- --no-audio /tmp/iron86-usage.txt
	./iron86 -h | grep -q -- --no-8087
	grep -q -- --floppy-b /tmp/iron86-usage.txt
	./iron86 -h | grep -q -- --floppy-b
	./iron86 -h | grep -q -- --floppy-a
	./iron86 --preset ibm-at >/tmp/iron86-at.out 2>/tmp/iron86-at.err; test $$? -eq 2
	grep -q 'not yet' /tmp/iron86-at.err
	./iron86 --preset 8286 >/tmp/iron86-8286.out 2>/tmp/iron86-8286.err; test $$? -eq 2
	grep -q 'not yet' /tmp/iron86-8286.err
	dd if=/dev/zero of=/tmp/iron86-hdc-smoke.img bs=512 count=1 status=none
	./iron86 tests/fixtures/hdcprobe.com --hdc /tmp/iron86-hdc-smoke.img \
		>/tmp/iron86-hdc-smoke.out 2>/tmp/iron86-hdc-smoke.err
	grep -q 'HDC0' /tmp/iron86-hdc-smoke.err
	grep -q 'CHS=306/4/17' /tmp/iron86-hdc-smoke.err
	grep -q 'LBA=20808' /tmp/iron86-hdc-smoke.err
	grep -q 'size=10MB' /tmp/iron86-hdc-smoke.err
	grep -q 'BIOS_DL=80h' /tmp/iron86-hdc-smoke.err
	grep -q 'AX=2F00' /tmp/iron86-hdc-smoke.err
	./iron86 tests/fixtures/hdcprobe.com >/tmp/iron86-hdc-none.out 2>/tmp/iron86-hdc-none.err
	grep -q 'AX=FFFF' /tmp/iron86-hdc-none.err
	rm -f /tmp/iron86-hdc.log
	./iron86 tests/fixtures/hdcprobe.com --hdc /tmp/iron86-hdc-smoke.img \
		--log /tmp/iron86-hdc.log >/tmp/iron86-hdc-log.out 2>/tmp/iron86-hdc-log.err
	grep -q 'HDC0' /tmp/iron86-hdc.log
	grep -q 'CHS=306/4/17' /tmp/iron86-hdc.log
	grep -q 'LBA=20808' /tmp/iron86-hdc.log
	grep -q 'size=10MB' /tmp/iron86-hdc.log
	grep -q 'BIOS_DL=80h' /tmp/iron86-hdc.log
	grep -q 'DOS=C:' /tmp/iron86-hdc.log
	grep -q 'HDC0' /tmp/iron86-hdc-log.err
	./iron86 --log >/tmp/iron86-log-need.out 2>/tmp/iron86-log-need.err; test $$? -eq 2
	grep -q 'needs FILE' /tmp/iron86-log-need.err
	./iron86 --log /no/such/iron86-log-dir/x.log tests/fixtures/hdcprobe.com \
		>/tmp/iron86-log-bad.out 2>/tmp/iron86-log-bad.err; test $$? -eq 1
	grep -q 'cannot open log' /tmp/iron86-log-bad.err
	./iron86 --log /tmp/iron86-hdc-order.log --hdc /tmp/iron86-hdc-smoke.img \
		tests/fixtures/hdcprobe.com >/tmp/iron86-hdc-order.out 2>/tmp/iron86-hdc-order.err
	grep -q 'HDC0' /tmp/iron86-hdc-order.log
	grep -q 'AX=2F00' /tmp/iron86-hdc-order.err
	! grep -q 'OUT 322' /tmp/iron86-hdc-smoke.err
	grep -q 'OUT 322' /tmp/iron86-hdc.log
	grep -q 'IN  321' /tmp/iron86-hdc.log
	! grep -q 'OUT 322' /tmp/iron86-hdc-log.err
	./iron86 tests/fixtures/hdcprobe.com --hdc /tmp/iron86-hdc-smoke.img --lock-mem \
		>/tmp/iron86-lock.out 2>/tmp/iron86-lock.err
	grep -q 'lock-mem ram' /tmp/iron86-lock.err
	grep -q 'lock-mem hdc' /tmp/iron86-lock.err
	grep -q 'AX=2F00' /tmp/iron86-lock.err
	! grep -q 'lock-mem ram' /tmp/iron86-hdc-smoke.err
	rm -f /tmp/iron86-int13.log
	./iron86 tests/fixtures/int13log.com --hdc /tmp/iron86-hdc-smoke.img \
		--log /tmp/iron86-int13.log >/tmp/iron86-int13.out 2>/tmp/iron86-int13.err
	grep -q 'INT13' /tmp/iron86-int13.log
	grep -q 'AH=00' /tmp/iron86-int13.log
	grep -q 'handled' /tmp/iron86-int13.log
	! grep -q 'INT13' /tmp/iron86-int13.err

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
		tests/run_config tests/run_hdc tests/run_log gmon.out
