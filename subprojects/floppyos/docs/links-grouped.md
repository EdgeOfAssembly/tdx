# LINKS.txt — Grouped for FloppyOS

Source: `/tmp/LINKS.txt` · Date: 2026-07-26  
Unique GitHub repos: ~56 · Non-GitHub: 4

---

## A. Memory managers & DOS extenders (Phase 2 drivers)

| URL | Notes |
|-----|--------|
| https://github.com/Baron-von-Riedesel/HimemX | XMS HIMEM replacement |
| https://github.com/Baron-von-Riedesel/Jemm | EMM386-class EMS/UMB |
| https://github.com/FDOS/emm386 | FreeDOS EMM386 |
| https://github.com/Baron-von-Riedesel/HX | HX DOS extender |
| https://github.com/Baron-von-Riedesel/Dos64-stub | 64-bit DOS stub |
| https://github.com/amindlost/dos32a | DOS/32A extender |
| https://github.com/yetmorecode/dos32a-ng | DOS/32A next-gen fork |

## B. Mouse / console / CD drivers

| URL | Notes |
|-----|--------|
| https://github.com/davidebreso/ctmouse | CuteMouse |
| https://github.com/FDOS/ansi | FreeDOS ANSI / NANSI docs |
| https://github.com/adoxa/shsucd | SHSUCDX CD-ROM |

## C. Toolchain & debug

| URL | Notes |
|-----|--------|
| https://github.com/tkchia/gcc-ia16 | GCC for IA-16 (roadmap P0) |
| https://github.com/Baron-von-Riedesel/JWasm | MASM-compatible assembler |
| https://github.com/Baron-von-Riedesel/DOS-debug | DEBUG.COM enhanced |
| https://github.com/phoenixthrush/Tiny-C-Compiler | Tiny C (size experiments) |

## D. Boot / FAT filesystems (FlopFS + FAT tracks)

| URL | Notes |
|-----|--------|
| https://github.com/alexfru/BootProg | Boot sector / loader |
| https://github.com/qihaiyan/fat12 | FAT12 |
| https://github.com/RPAnimation/FAT12 | FAT12 |
| https://github.com/variousCodingTasks/FAT12 | FAT12 |
| https://github.com/MightyPork/fat16 | FAT16 |
| https://github.com/rweichler/FAT16 | FAT16 + layout PDF |
| https://github.com/Daste745/fat16 | FAT16 |

## E. Video / VESA / graphics

| URL | Notes |
|-----|--------|
| https://shawnhargreaves.com/freebe/ | FreeBE/AF (non-GH) |
| https://shawnhargreaves.com/freebe/freebs12.zip | FreeBE binary zip |
| https://github.com/qemu/vgabios | VBE in VGA BIOS |
| https://github.com/PluMGMK/vbesvga.drv | VBE SVGA driver |
| https://github.com/TolgaBagci/scitech-display-doctor-7 | UniVBE-era SciTech |
| https://github.com/Cacodemon345/VSBHDASF | Sound? / VSB related |
| https://github.com/ianmicheal/DCAllegroVersion-4.2.2 | Allegro (Dreamcast port lineage) |
| https://wiki.osdev.org/Accelerated_Graphic_Cards | OSDev wiki |

## F. Sound / MIDI / MT-32 / GUS

| URL | Notes |
|-----|--------|
| https://github.com/munt/munt | MT-32 emulator |
| https://github.com/dwhinham/mt32-pi | MT-32 on Pi |
| https://github.com/nukeykt/Nuked-SC55 | SC-55 emulator |
| https://github.com/skjelten/emusc | SC-55 related |
| https://github.com/tebl/BulkyMIDI-32 | MIDI hardware |
| https://github.com/kingbonj/GUS-Timidity | GUS + TiMidity |
| https://github.com/m13253/audio-scripts | TiMidity cfg |
| https://github.com/topics/mt-32 | Topic index |
| https://github.com/topics/roland-mt-32 | Topic index |
| https://gist.github.com/rubenerd/f92e1c10258ff083cd1e9b71f674e1c9 | Gist (check content) |

## G. CGA / EGA hardware & simulation

| URL | Notes |
|-----|--------|
| https://github.com/dbalsom/cga_sim | CGA simulator |
| https://github.com/dbalsom/cga_artifact_color | CGA artifact color |
| https://github.com/MobyGamer/CGACompatibilityTester | CGA compat tester |
| https://github.com/hkzlab/CGA_Schematics | CGA schematics |
| https://github.com/hkzlab/CGA_Redux | CGA Redux |
| https://github.com/schlae/EGACard | EGA card |
| https://github.com/schlae/EGAMemory | EGA memory |
| https://github.com/fjvva/IBM-5155-EGA-Adapter | EGA adapter |
| https://github.com/necroware/mce-adapter | MCE adapter |
| https://github.com/eliotw/IBM_PC | CGA ref PDF path |

## H. BIOS / firmware reference

| URL | Notes |
|-----|--------|
| https://github.com/gawlas/IBM-PC-BIOS | IBM PC BIOS |
| https://github.com/philspil66/IBM-PC-BIOS | IBM PC BIOS (+ PCBIOS.ASM) |
| https://github.com/ricardoquesada/bios-8088 | 8088 BIOS |
| https://github.com/virtualxt/pcxtbios | PC/XT BIOS |
| https://github.com/kaneton/appendix-bios | BIOS appendix |
| https://abdess.github.io/retrobios/systems/ibm/ | RetroBIOS catalog |

## I. Optical / mass storage / IDE emulation (host HW)

| URL | Notes |
|-----|--------|
| https://github.com/libcdio/libcdio | CD I/O library (host) |
| https://github.com/ZuluIDE/ZuluIDE-firmware | IDE optical emulator |
| https://github.com/RibShark/OmniDrive | Drive emulator |
| https://github.com/slzKud/DiscEmu-luckfox-pico-mini | Disc emulator (Luckfox) |

## J. Shell / alternate OS / CP/M

| URL | Notes |
|-----|--------|
| https://github.com/Arquivotheca/4DOS-src | 4DOS shell sources |
| https://github.com/brouhaha/cpm22 | CP/M 2.2 |
| https://github.com/davidgiven/cpmish | CP/M-ish |
| https://github.com/Tandy2K/Tandy2000 | Tandy 2000 |

## K. FloppyOS priority ranking (for subprojects now)

### P0 — helps FloppyOS immediately
1. HimemX, Jemm / FDOS emm386  
2. CuteMouse (ctmouse)  
3. SHSUCDX (shsucd)  
4. gcc-ia16, JWasm  
5. BootProg + FAT12/16 refs  
6. FreeBE/AF (VESA)  
7. DOS-debug  

### P1 — soon after bootable core
- HX, DOS/32A / dos32a-ng  
- NANSI (FDOS/ansi)  
- 4DOS (shell ideas; license!)  
- VBE/SciTech references  

### P2 — nice later (hardware, sound, CGA/EGA, BIOS ROMs)
- munt, mt32-pi, Nuked-SC55, GUS-Timidity  
- CGA/EGA projects  
- IBM-PC-BIOS trees  
- ZuluIDE / OmniDrive / DiscEmu  
- CP/M, Tandy2000, Tiny-C  

---

*Grouped for agent follow-up: version/fork audit + subproject scaffolding.*
