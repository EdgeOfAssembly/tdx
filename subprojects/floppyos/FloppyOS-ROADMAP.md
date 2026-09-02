# FloppyOS Project Roadmap

**Project Goal**: A modernized, game-friendly, floppy-oriented DOS based on open MS-DOS 4.x sources. Focus on maximum conventional memory, automatic handling of problematic games, optimized built-in drivers, and excellent out-of-the-box support for period hardware and games, while remaining usable on real and emulated 8086-era systems and extending to modern hardware where practical.

**Status**: Phase 1 research complete. Ready for implementation.

**Repository**: https://github.com/EdgeOfAssembly/FloppyOS

---

## Phase 1 – Core System (Immediate Priority)
**Success Criteria**: Produce working `IO.SYS`, `MSDOS.SYS`, and `COMMAND.COM` from MS-DOS 4.x sources that boot and run correctly on real or emulated IBM-era hardware (8086/8088–386).

**Sources**:
- Primary: https://github.com/microsoft/MS-DOS (v1.25, v2.0, v4.0 under MIT)
- Early: https://github.com/DOS-History/Paterson-Listings (86-DOS 1.00)
- Modern assembler adaptation: https://github.com/AndresTraks/MS-DOS-FASM

**Toolchain Preference**:
1. OpenWatcom (best for 16-bit real-mode C/.COM/.EXE)
2. GCC-IA16 (tkchia) for pure 16-bit code
3. DOSBox + period tools / modern UASM/JWasm as fallback

**Known Build Notes**: Apply CRLF conversion, fix Unicode replacement characters, and correct paths in SETENV.BAT.

---

## Phase 2 – Gaming-Oriented Enhancements + Modern Compatibility Drivers

### Smart Defaults & Game Profiles
- Automatic optimized CONFIG.SYS / AUTOEXEC.BAT maximizing conventional memory.
- User files override if present.
- Game EXE hashing (XXH32/CRC32) + auto-reboot into tailored profiles (e.g. Ultima VII pure real-mode).

### Classic Optimized Drivers (high-loaded)
- HimemX (XMS)
- CuteMouse
- NANSI.SYS
- SHSUCDX + tiny CD driver
- VESA 1.x/2.x (FreeBE/AF + UniVBE 5.2 sources)
- Sound: SB Pro 2.0 / SB16 / AWE family, GUS, Roland MPU (SoftMPU)

### New: USB Support
- **Hardware reality**: USB 3.x ports are backward-compatible with USB 2.0/1.1 devices (and controllers often expose EHCI).
- **Software**: Pure real-mode DOS has workable USB 1.1/2.0 mass-storage stacks (USBASPI.SYS family, DI1000DD, FreeDOS usbdos, USBDDOS project).
- Full native xHCI (USB 3) is impractical in pure 16-bit real mode; rely on BIOS or EHCI fallback.
- Include as optional high-loaded or profile-activated mass-storage / HID drivers for flash drives and keyboards/mice.

### New: Modern HDD / SATA with LBA
- MS-DOS 4 is CHS-centric and limited for large drives.
- Priority: Full INT 13h Extensions (LBA) support so the system works with modern BIOS-provided large disks.
- SATA: Prefer BIOS legacy/IDE mode or INT 13h. Pure AHCI is difficult; experimental drivers or later protected-mode helpers.
- Ties strongly into Phase 3 FAT32 for usable large partitions.
- Goal: Make FloppyOS practical on modern hardware with large SATA SSDs/HDDs while keeping pure 8086/floppy scenarios clean.

### Later Phase 2: Built-in DPMI (CWSDPMI preferred, conflict handling for DOS/4GW games).

---

## Phase 3 – Advanced Filesystems
- FAT32 (high priority, pairs with LBA support)
- ISO / CUE+BIN image mounting
- UDF experimental
- Limited read-only Linux FS support (ext2/SquashFS etc.) later

---

## Phase 4 – Linux Ports (Byproduct)
- Enhanced COMMAND.COM for Linux with drive mapping, dual-slash support, case sensitivity, long filenames (base on FreeCOM).
- EXE2BIN, EDIT, and selected utilities.

---

## Testing Strategy
- **DOSBox-X**: Daily development, rapid testing.
- **QEMU**: Full boot images and broader hardware.
- **86Box / PCem**: Final accuracy for ISA cards, timing, sound, VESA, and game profiles.
- Real hardware for ultimate validation.

---

## Key Principles
- Size optimization and high-loading critical.
- Prefer high-quality open-source components.
- Keep pure real-mode / 8086 compatibility for core + classic games.
- Extend gracefully to modern hardware (LBA, USB mass storage, large disks) without breaking the classic experience.

**Document updated**: July 24, 2026 (added USB + LBA/SATA)
**Team**: Grok (lead), Benjamin, Lucas, Harper

Ready for Phase 1 implementation.
