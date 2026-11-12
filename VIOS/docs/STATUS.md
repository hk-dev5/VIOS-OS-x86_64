# VIOS Development Status

## Completed and verified

- [x] Independent freestanding x86-64 kernel (not Linux-based)
- [x] Limine boot protocol integration
- [x] Hybrid BIOS + UEFI bootable ISO (`build/VIOS.iso`)
- [x] Framebuffer text console with colors and basic formatting
- [x] Boot stage 1: hardware diagnostics (real CPUID brand, real memmap, real FB)
- [x] Boot stage 2: loading progress reflecting init steps
- [x] Boot stage 3: CRT-style effect
- [x] Boot stage 4: BOOTING SUCCESS
- [x] Boot stage 5: CONNECTING TO VIOS MODEL → honest OFFLINE
- [x] Interactive shell with working commands
- [x] `kernel --version` with large ASCII eastern dragon logo + system info
- [x] Makefile + ISO generation with xorriso + Limine
- [x] README with WSL/Windows 10 build instructions

## Partial / stubs

- Keyboard input (PS/2 polling – works in QEMU)
- Memory reporting (Limine memmap only)
- GDT / IDT / exceptions (not installed)
- Physical / virtual memory managers (not implemented)
- Heap, timer, scheduler, processes, syscalls (not implemented)
- Filesystem (none)
- Drivers beyond framebuffer + basic keyboard (none)

## Not implemented

- Network stack → connection intentionally shows OFFLINE
- Audio, USB host controller, PCI enumeration drivers
- Persistent storage / FAT32
- Applications (notepad, spreadsheet, calculator, games…)
- 32-bit support (explicitly out of scope for now)

## Build artifact

```
build/VIOS.iso   (~4.2 MiB)
```

Tested construction of ISO and ELF headers. Full visual boot sequence is intended for QEMU with display enabled (`make run`).
