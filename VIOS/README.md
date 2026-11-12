# VIOS – Independent Operating System

**VIOS** is an independent x86-64 operating system with its own freestanding kernel.
It is **not** a renamed Linux distribution.

Current status: **early development / minimal bootable kernel**.

## Features implemented (v0.1.0-dev)

| Component              | Status      | Notes                                      |
|------------------------|-------------|--------------------------------------------|
| Limine boot protocol   | IMPLEMENTED | UEFI + BIOS hybrid ISO                     |
| Framebuffer console    | IMPLEMENTED | 8x16 font, colors, basic printf            |
| Hardware diagnostics   | PARTIAL     | Real CPU brand, memmap, FB; others SKIP    |
| Loading bar            | IMPLEMENTED | Reflects init stages                       |
| CRT effect             | IMPLEMENTED | Simple noise/flicker                       |
| Boot success           | IMPLEMENTED |                                            |
| VIOS MODEL connect     | HONEST      | Shows OFFLINE (no network stack yet)       |
| Shell                  | PARTIAL     | help, clear, uname, uptime, free, kernel…  |
| kernel --version       | IMPLEMENTED | ASCII eastern dragon + version info        |
| Physical memory info   | IMPLEMENTED | From Limine memmap                         |
| PS/2 keyboard polling  | PARTIAL     | Works under QEMU                           |
| GDT / IDT / PMM / VMM  | STUB        | Not yet real implementations               |
| Scheduler / processes  | NOT IMPL.   |                                            |
| Filesystem (FAT32/RAM) | STUB        |                                            |
| Drivers (USB, net…)    | NOT IMPL.   |                                            |
| Applications           | NOT IMPL.   |                                            |

## Project layout

```
VIOS/
├── boot/           # limine.conf
├── kernel/
│   ├── include/    # console.h, kernel.h
│   └── src/        # console.c, kernel.c
├── include/        # limine.h
├── linker/         # linker.ld
├── build/          # output (VIOS.iso, vios.elf)
├── Makefile
└── README.md
```

## Build requirements

- GCC (with freestanding support, x86-64)
- GNU Make, ld, nasm (optional)
- xorriso
- Limine binary package (included or downloaded)
- QEMU for testing

### Build on Ubuntu / WSL (Windows 10)

1. **Enable / install WSL + Ubuntu**
   ```powershell
   wsl --install -d Ubuntu
   ```
   Restart if needed, then open Ubuntu.

2. **Install dependencies**
   ```bash
   sudo apt update
   sudo apt install -y build-essential nasm xorriso qemu-system-x86 git curl
   ```

3. **Get Limine binaries** (already present next to the project in this environment)
   ```bash
   # If missing:
   curl -L -o limine-binary.tar.gz \
     https://github.com/Limine-Bootloader/Limine/releases/download/v12.9.6/limine-binary.tar.gz
   mkdir -p limine && tar xzf limine-binary.tar.gz -C limine --strip-components=1
   cd limine && make && cd ..
   ```

4. **Build**
   ```bash
   cd VIOS
   make clean
   make
   ```
   Output: `build/VIOS.iso`

5. **Verify ISO exists**
   ```bash
   ls -lh build/VIOS.iso
   ```

## Run with QEMU

```bash
make run
# or
qemu-system-x86_64 -m 256M -cdrom build/VIOS.iso
```

Expected sequence:
1. Hardware diagnostics (CPU, MEMORY, GPU OK; optional devices SKIP)
2. Loading bar 0–100%
3. Short CRT noise effect
4. “BOOTING SUCCESS ….”
5. CONNECTING TO VIOS MODEL → OFFLINE (honest)
6. Shell prompt `vios>`

Useful commands inside shell:
```
help
kernel --version
uname
free
clear
```

## Write ISO to USB (real hardware)

**Warning:** This erases all data on the target USB.

- **Rufus** (Windows): select the ISO, GPT/UEFI or MBR/BIOS according to your machine, write.
- **balenaEtcher**: select ISO → select USB → Flash.
- Linux: `dd if=build/VIOS.iso of=/dev/sdX bs=4M status=progress && sync`

Then boot from USB (usually F12 / Esc / F10 boot menu).

## Known limitations (honest)

- No real GDT/IDT installation yet (still running with Limine’s environment).
- No preemptive scheduler or multi-process support.
- No persistent filesystem; everything is in RAM for now.
- Network, audio, USB host, storage drivers: not present.
- Keyboard is simple PS/2 polling; may not work on all real hardware.
- “VIOS MODEL” connection deliberately reports OFFLINE because there is no network stack.
- Only a small set of shell commands is implemented.

## Next development priorities

1. Proper GDT + IDT + exception handlers
2. Physical memory manager (bitmap / buddy)
3. Virtual memory / higher-half clean mapping
4. Heap allocator
5. Timer + basic scheduler
6. RAM filesystem + simple VFS
7. ATA / AHCI or USB mass-storage driver
8. Expand shell and add more utilities

## License

Code written for this project is provided as-is for educational and research purposes.
Limine is under its own BSD-style license.

---
VIOS 0.1.0-dev – independent, freestanding, bootable.
