# TDX / librex — debug by default (gnu-make + linker-dynamic).
CXX := g++ -Wl,--as-needed
CC  := gcc -Wl,--as-needed
MAKEFLAGS += --no-print-directory

export PKG_CONFIG_PATH ?= $(HOME)/.local/share/pkgconfig:$(HOME)/.local/lib64/pkgconfig:$(HOME)/.local/lib/pkgconfig:$(PKG_CONFIG_PATH)

SDL_CFLAGS := $(shell pkg-config --cflags sdl2)
SDL_LIBS   := $(shell pkg-config --libs sdl2)
PKG_CS  := $(shell pkg-config --cflags --libs capstone)
PKG_UC  := $(shell pkg-config --cflags --libs unicorn)
CATCH_CFLAGS := $(shell . $(HOME)/.local/share/test-frameworks/env.sh >/dev/null 2>&1; pkg-config --cflags catch2-with-main)
CATCH_LIBS   := $(shell . $(HOME)/.local/share/test-frameworks/env.sh >/dev/null 2>&1; pkg-config --libs catch2-with-main)

CXXFLAGS_COMMON := -std=gnu++23 -Wall -Wextra -Wpedantic -Wshadow -Wconversion \
	-fno-omit-frame-pointer -Iinclude
CFLAGS_COMMON := -std=gnu23 -Wall -Wextra -Wpedantic -Wshadow -Wconversion \
	-fno-omit-frame-pointer -Iinclude

# Header deps are still generated (-MMD -MP) so `make` knows what to rebuild,
# but binaries are linked from ONE gulp of gcc — a single invocation compiles
# every source file together. A stale/ABI-mismatched .o can never exist again:
# either the whole program compiles from the current tree, or it fails loudly.
DEPFLAGS := -MMD -MP
CXXFLAGS_COMMON += $(DEPFLAGS)
CFLAGS_COMMON += $(DEPFLAGS)

CXXFLAGS_OPTIMIZED := -O3 -march=x86-64 -mtune=generic -fno-omit-frame-pointer
CXXFLAGS_DEBUG := $(CXXFLAGS_COMMON) -g3 -O0 -fsanitize=address,undefined -rdynamic
LDFLAGS_DEBUG  := -fsanitize=address,undefined -rdynamic
CXXFLAGS_RELEASE := $(CXXFLAGS_COMMON) -DNDEBUG $(CXXFLAGS_OPTIMIZED)
LDFLAGS_RELEASE  :=

CXXFLAGS := $(CXXFLAGS_DEBUG)
LDFLAGS  := $(LDFLAGS_DEBUG)
CFLAGS   := $(CFLAGS_COMMON) -g3 -O0 -fsanitize=address,undefined

BUILD_FLAGS := -s V=0 -j$(shell nproc 2>/dev/null || echo 1)

REX_SRC := src/rex/rex_log.cpp src/rex/rex_disasm.cpp src/rex/rex_session.cpp src/rex/rex_sock.cpp \
	src/dos/dos_machine.cpp src/dos/dos_int.cpp src/dos/mz_parse.c src/dos/dos_cga.c
TDX_SRC := src/tdx/tdx_cli.cpp src/tdx/tdx_font.cpp src/tdx/tdx_ui.cpp src/tdx/tdx_main.cpp \
	src/tdx/tdx_shot.cpp
VIEW_SRC := src/tdx/tdx_view.cpp src/tdx/tdx_font.cpp src/tdx/tdx_shot.cpp src/tdx/tdx_agent_sock.cpp
TEST_SRCS := tests/test_mz.cpp tests/test_cga.cpp tests/test_step.cpp tests/test_cli.cpp tests/test_bp.cpp \
	tests/test_shot.cpp

PY := $(shell if [ -x /mnt/python/bin/python ]; then echo /mnt/python/bin/python; else echo python3; fi)

.PHONY: all clean test tests verify fixtures release profile install

all: tdx tdxview

# One-gulp builds: feed every source to g++ in a single invocation so the whole
# program is always compiled and linked from the same snapshot of the tree.
# There are no intermediate .o files at all, so a stale/ABI-mismatched object
# (the class of bug that segfaulted tdx inside handle_int16) cannot occur.
# The .c sources are compiled as C++ here (their public functions use the
# REX_C_DEF dual-linkage macro); they are still plain C23 for CBMC `verify`.

tdx: $(REX_SRC) $(TDX_SRC)
	$(CXX) $(CXXFLAGS) $(SDL_CFLAGS) $(LDFLAGS) -o $@ $(REX_SRC) $(TDX_SRC) \
		$(SDL_LIBS) $(PKG_CS) $(PKG_UC)

tdxview: $(VIEW_SRC)
	$(CXX) $(CXXFLAGS) $(SDL_CFLAGS) $(LDFLAGS) -o $@ $(VIEW_SRC) $(SDL_LIBS)

tests/run_tests: $(REX_SRC) $(TEST_SRCS) src/tdx/tdx_cli.cpp src/tdx/tdx_shot.cpp fixtures
	$(CXX) $(CXXFLAGS) $(CATCH_CFLAGS) $(LDFLAGS) -o $@ $(REX_SRC) $(TEST_SRCS) \
		src/tdx/tdx_cli.cpp src/tdx/tdx_shot.cpp \
		$(CATCH_LIBS) $(PKG_CS) $(PKG_UC)

# Track every header as a dependency so `make` rebuilds the (single-object)
# binaries whenever any header changes — with one-gulp that's automatic and
# always correct, just a little slower. List them once here.
HEADERS := $(shell find include -name '*.h' 2>/dev/null)
tdx tdxview tests/run_tests: $(HEADERS)

tests/fixtures/tiny.com: tests/fixtures/tiny.asm
	nasm -f bin -o $@ $<

tests/fixtures/over.com: tests/fixtures/over.asm
	nasm -f bin -o $@ $<

tests/fixtures/loop.com: tests/fixtures/loop.asm
	nasm -f bin -o $@ $<

tests/fixtures/far.com: tests/fixtures/far.asm
	nasm -f bin -o $@ $<

tests/fixtures/setblock.com: tests/fixtures/setblock.asm
	nasm -f bin -o $@ $<

tests/fixtures/int3pad.com: tests/fixtures/int3pad.asm
	nasm -f bin -o $@ $<

tests/fixtures/fcbopen.com: tests/fixtures/fcbopen.asm
	nasm -f bin -o $@ $<

tests/fixtures/waitkey.com: tests/fixtures/waitkey.asm
	nasm -f bin -o $@ $<

tests/fixtures/int16spin.com: tests/fixtures/int16spin.asm
	nasm -f bin -o $@ $<

fixtures: tests/fixtures/tiny.com tests/fixtures/over.com tests/fixtures/loop.com tests/fixtures/far.com \
	tests/fixtures/setblock.com tests/fixtures/int3pad.com tests/fixtures/fcbopen.com \
	tests/fixtures/waitkey.com tests/fixtures/int16spin.com

test: tdx tdxview tests/run_tests
	./tests/run_tests
	./tdx -h >/dev/null
	./tdx -v
	./tdx --no-ui --no-sock tests/fixtures/tiny.com >/dev/null
	./tdxview -h >/dev/null
	./tdxview -v
	$(PY) scripts/tdxctl.py -h >/dev/null
	$(PY) scripts/tdxctl.py -v
	@rm -f /tmp/tdx-test.sock
	./tdx --no-ui tests/fixtures/tiny.com --sock /tmp/tdx-test.sock >/tmp/tdx-test.log 2>&1 & \
	  echo $$! > /tmp/tdx-test.pid; \
	  sleep 0.3; \
	  $(PY) scripts/tdxctl.py --sock /tmp/tdx-test.sock cga | grep -q pixels_b64; \
	  $(PY) scripts/tdxctl.py --sock /tmp/tdx-test.sock quit; \
	  wait $$(cat /tmp/tdx-test.pid) 2>/dev/null || true
tests: test

verify: test
	$(HOME)/.local/bin/cbmc formal/verify_mz.c src/dos/mz_parse.c -Iinclude \
		--bounds-check --pointer-check --unwind 40 --unwinding-assertions

clean:
	rm -f tdx tdxview tests/run_tests \
		tests/fixtures/tiny.com tests/fixtures/over.com tests/fixtures/loop.com \
		tests/fixtures/far.com tests/fixtures/setblock.com tests/fixtures/int3pad.com \
		tests/fixtures/fcbopen.com tests/fixtures/waitkey.com tests/fixtures/int16spin.com \
		*.d src/**/*.d tests/**/*.d

release:
	$(MAKE) $(BUILD_FLAGS) clean CXXFLAGS="$(CXXFLAGS_RELEASE)" \
		CFLAGS="$(CFLAGS_COMMON) -O3 -DNDEBUG" LDFLAGS="$(LDFLAGS_RELEASE)" all

profile:
	$(MAKE) $(BUILD_FLAGS) clean \
		CXXFLAGS="$(CXXFLAGS_COMMON) -DNDEBUG $(CXXFLAGS_OPTIMIZED) -g -pg -fno-inline" \
		LDFLAGS="-pg" all

install: tdx tdxview
	install -m 755 tdx /usr/local/bin/tdx
	install -m 755 tdxview /usr/local/bin/tdxview
	install -m 755 scripts/tdxctl.py /usr/local/bin/tdxctl
