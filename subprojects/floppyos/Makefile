# FloppyOS host build
BUILD_DIR ?= build
CC       ?= gcc
CFLAGS   ?= -std=c23 -Wall -Wextra -Wpedantic -O2
NASM     ?= nasm
PYTHON   ?= python3

.PHONY: all boot stage15 kernel programs tools image smoke smoke-py86 run-py86 test clean size help

all: image

help:
	@echo "Targets: image smoke test size clean"
	@echo "BUILD_DIR=$(BUILD_DIR)"

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/boot.bin: boot/boot.asm | $(BUILD_DIR)
	$(NASM) -f bin -o $@ $<
	@test $$(wc -c < $@) -eq 512

$(BUILD_DIR)/stage1_5.bin: boot/stage1_5.asm | $(BUILD_DIR)
	$(NASM) -f bin -o $@ $<
	@test $$(wc -c < $@) -eq 1024

$(BUILD_DIR)/kernel.bin: kernel/kernel.asm | $(BUILD_DIR)
	$(NASM) -f bin -o $@ $<
	@test $$(wc -c < $@) -eq 16384

$(BUILD_DIR)/hello.com: programs/hello.asm | $(BUILD_DIR)
	$(NASM) -f bin -o $@ $<

$(BUILD_DIR)/command.com: programs/command.asm | $(BUILD_DIR)
	$(NASM) -f bin -o $@ $<

$(BUILD_DIR)/hello.exe: tools/mkhello_exe.py | $(BUILD_DIR)
	python3 tools/mkhello_exe.py
	cp -a programs/hello.exe $@

$(BUILD_DIR)/mkimg: tools/mkimg.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/mkflopfs: tools/mkflopfs.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $<

tools: $(BUILD_DIR)/mkimg $(BUILD_DIR)/mkflopfs
$(BUILD_DIR)/tiny.com: programs/tiny.asm | $(BUILD_DIR)
	$(NASM) -f bin -o $@ $<

$(BUILD_DIR)/fcbtest.com: programs/fcbtest.asm | $(BUILD_DIR)
	$(NASM) -f bin -o $@ $<

programs: $(BUILD_DIR)/hello.com $(BUILD_DIR)/tiny.com $(BUILD_DIR)/fcbtest.com
image: $(BUILD_DIR)/floppyos.img

$(BUILD_DIR)/floppyos.img: $(BUILD_DIR)/boot.bin $(BUILD_DIR)/stage1_5.bin \
		$(BUILD_DIR)/kernel.bin $(BUILD_DIR)/hello.com $(BUILD_DIR)/command.com \
		$(BUILD_DIR)/hello.exe $(BUILD_DIR)/tiny.com $(BUILD_DIR)/fcbtest.com \
		$(BUILD_DIR)/mkimg $(BUILD_DIR)/mkflopfs
	$(BUILD_DIR)/mkimg -s 360 -o $@ -b $(BUILD_DIR)/boot.bin
	$(BUILD_DIR)/mkflopfs -i $@ -s $(BUILD_DIR)/stage1_5.bin \
		-k $(BUILD_DIR)/kernel.bin -c $(BUILD_DIR)/command.com \
		-f $(BUILD_DIR)/hello.com -f $(BUILD_DIR)/hello.exe \
		-f $(BUILD_DIR)/tiny.com -f $(BUILD_DIR)/fcbtest.com
	@test $$(wc -c < $@) -eq 368640

smoke: smoke-py86

smoke-interactive: image
	python3 tests/smoke_interactive.py $(BUILD_DIR)/floppyos.img

test: smoke-py86

size: image
	@wc -c $(BUILD_DIR)/boot.bin $(BUILD_DIR)/stage1_5.bin \
		$(BUILD_DIR)/kernel.bin $(BUILD_DIR)/hello.com $(BUILD_DIR)/floppyos.img


# Preferred host test: local Py86 copy (does NOT use monorepo Py86)
# Interactive MDA in terminal — safe for this host (avoid QEMU SDL hangs)
run-py86: image
	cd subprojects/py86 && $(PYTHON) py86.py \
		--config ../../tests/py86_floppyos.json \
		--display term --fast-post --steps 20000000

smoke-py86: image
	python3 tests/smoke_py86.py

# QEMU optional — has hung this Gentoo host; use only if you accept the risk
smoke-qemu: image
	@echo "WARNING: QEMU has hung this machine before. Prefer: make smoke-py86" >&2
	python3 tests/smoke_interactive.py $(BUILD_DIR)/floppyos.img

# Optional 1.44MB image (not for Py86/5150)
image1440: $(BUILD_DIR)/boot.bin $(BUILD_DIR)/stage1_5.bin \
		$(BUILD_DIR)/kernel.bin $(BUILD_DIR)/hello.com $(BUILD_DIR)/command.com \
		$(BUILD_DIR)/hello.exe $(BUILD_DIR)/mkimg $(BUILD_DIR)/mkflopfs
	$(BUILD_DIR)/mkimg -s 1440 -o $(BUILD_DIR)/floppyos1440.img -b $(BUILD_DIR)/boot.bin
	$(BUILD_DIR)/mkflopfs -i $(BUILD_DIR)/floppyos1440.img -s $(BUILD_DIR)/stage1_5.bin \
		-k $(BUILD_DIR)/kernel.bin -c $(BUILD_DIR)/command.com \
		-f $(BUILD_DIR)/hello.com -f $(BUILD_DIR)/hello.exe

clean:
	rm -rf $(BUILD_DIR)
