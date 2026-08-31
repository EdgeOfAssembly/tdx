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

CXXFLAGS_OPTIMIZED := -O3 -march=x86-64 -mtune=generic -fno-omit-frame-pointer
CXXFLAGS_DEBUG := $(CXXFLAGS_COMMON) -g3 -O0 -fsanitize=address,undefined -rdynamic
LDFLAGS_DEBUG  := -fsanitize=address,undefined -rdynamic
CXXFLAGS_RELEASE := $(CXXFLAGS_COMMON) -DNDEBUG $(CXXFLAGS_OPTIMIZED)
LDFLAGS_RELEASE  :=

CXXFLAGS := $(CXXFLAGS_DEBUG)
LDFLAGS  := $(LDFLAGS_DEBUG)
CFLAGS   := $(CFLAGS_COMMON) -g3 -O0 -fsanitize=address,undefined

BUILD_FLAGS := -s V=0 -j$(shell nproc 2>/dev/null || echo 1)

REX_CXX := src/rex/rex_log.cpp src/rex/rex_disasm.cpp src/rex/rex_session.cpp src/rex/rex_sock.cpp
DOS_CXX := src/dos/dos_machine.cpp src/dos/dos_int.cpp
DOS_C   := src/dos/mz_parse.c src/dos/dos_cga.c
TDX_CXX := src/tdx/tdx_cli.cpp src/tdx/tdx_font.cpp src/tdx/tdx_ui.cpp src/tdx/tdx_main.cpp \
	src/tdx/tdx_shot.cpp

REX_OBJS := $(REX_CXX:.cpp=.o) $(DOS_CXX:.cpp=.o) $(DOS_C:.c=.o)
TDX_OBJS := $(TDX_CXX:.cpp=.o)

TEST_SRCS := tests/test_mz.cpp tests/test_cga.cpp tests/test_step.cpp tests/test_cli.cpp tests/test_bp.cpp \
	tests/test_shot.cpp

PY := $(shell if [ -x /mnt/python/bin/python ]; then echo /mnt/python/bin/python; else echo python3; fi)

VIEW_OBJS := src/tdx/tdx_view.o src/tdx/tdx_font.o src/tdx/tdx_shot.o src/tdx/tdx_agent_sock.o

.PHONY: all clean test tests verify fixtures release profile install

all: tdx tdxview

src/dos/%.o: src/dos/%.c
	$(CC) $(CFLAGS) -c $< -o $@

src/dos/%.o: src/dos/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

src/rex/%.o: src/rex/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

src/tdx/%.o: src/tdx/%.cpp
	$(CXX) $(CXXFLAGS) $(SDL_CFLAGS) -c $< -o $@

tdx: $(REX_OBJS) $(TDX_OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(REX_OBJS) $(TDX_OBJS) $(SDL_LIBS) $(PKG_CS) $(PKG_UC)

tdxview: $(VIEW_OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(VIEW_OBJS) $(SDL_LIBS)

tests/run_tests: $(REX_OBJS) src/tdx/tdx_cli.o src/tdx/tdx_shot.o $(TEST_SRCS) fixtures
	$(CXX) $(CXXFLAGS) $(CATCH_CFLAGS) $(LDFLAGS) -o $@ $(TEST_SRCS) src/tdx/tdx_cli.o \
		src/tdx/tdx_shot.o $(REX_OBJS) \
		$(CATCH_LIBS) $(PKG_CS) $(PKG_UC)

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

fixtures: tests/fixtures/tiny.com tests/fixtures/over.com tests/fixtures/loop.com tests/fixtures/far.com \
	tests/fixtures/setblock.com tests/fixtures/int3pad.com tests/fixtures/fcbopen.com \
	tests/fixtures/waitkey.com

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
	rm -f tdx tdxview tests/run_tests $(REX_OBJS) $(TDX_OBJS) $(VIEW_OBJS) \
		tests/fixtures/tiny.com tests/fixtures/over.com tests/fixtures/loop.com \
		tests/fixtures/far.com tests/fixtures/setblock.com tests/fixtures/int3pad.com \
		tests/fixtures/fcbopen.com tests/fixtures/waitkey.com

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
