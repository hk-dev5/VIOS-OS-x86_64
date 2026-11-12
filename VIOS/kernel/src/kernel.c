#include "kernel.h"
#include "console.h"
#include <limine.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

extern volatile struct limine_framebuffer_request framebuffer_request;

/* Limine requests */
__attribute__((used, section(".limine_requests")))
static volatile uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(3);

__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_executable_address_request executable_address_request = {
    .id = LIMINE_EXECUTABLE_ADDRESS_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests_start")))
static volatile uint64_t limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
static volatile uint64_t limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;

/* Simple delay (busy loop) */
static void delay(volatile uint64_t count) {
    while (count--) {
        __asm__ volatile ("pause");
    }
}

static uint64_t uptime_ticks = 0;

uint64_t get_uptime_ticks(void) {
    return uptime_ticks;
}

uint64_t get_total_memory(void) {
    if (memmap_request.response == NULL) return 0;
    uint64_t total = 0;
    for (uint64_t i = 0; i < memmap_request.response->entry_count; i++) {
        struct limine_memmap_entry *e = memmap_request.response->entries[i];
        if (e->type == LIMINE_MEMMAP_USABLE ||
            e->type == LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE ||
            e->type == LIMINE_MEMMAP_ACPI_RECLAIMABLE ||
            e->type == LIMINE_MEMMAP_EXECUTABLE_AND_MODULES) {
            total += e->length;
        }
    }
    return total;
}

uint64_t get_usable_memory(void) {
    if (memmap_request.response == NULL) return 0;
    uint64_t usable = 0;
    for (uint64_t i = 0; i < memmap_request.response->entry_count; i++) {
        struct limine_memmap_entry *e = memmap_request.response->entries[i];
        if (e->type == LIMINE_MEMMAP_USABLE) {
            usable += e->length;
        }
    }
    return usable;
}

const char *get_cpu_brand(void) {
    static char brand[49] = "Unknown CPU";
    uint32_t regs[4];
    /* CPUID leaf 0x80000000 */
    __asm__ volatile ("cpuid" : "=a"(regs[0]), "=b"(regs[1]), "=c"(regs[2]), "=d"(regs[3]) : "a"(0x80000000));
    if (regs[0] < 0x80000004) return brand;
    char *p = brand;
    for (uint32_t leaf = 0x80000002; leaf <= 0x80000004; leaf++) {
        __asm__ volatile ("cpuid" : "=a"(regs[0]), "=b"(regs[1]), "=c"(regs[2]), "=d"(regs[3]) : "a"(leaf));
        *(uint32_t *)p = regs[0]; p += 4;
        *(uint32_t *)p = regs[1]; p += 4;
        *(uint32_t *)p = regs[2]; p += 4;
        *(uint32_t *)p = regs[3]; p += 4;
    }
    brand[48] = 0;
    /* Trim leading spaces */
    p = brand;
    while (*p == ' ') p++;
    if (p != brand) {
        char *q = brand;
        while (*p) *q++ = *p++;
        *q = 0;
    }
    return brand;
}

__attribute__((noreturn)) void hcf(void) {
    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}

/* ========== Boot Stage 1: Hardware diagnostics ========== */
static void stage_diagnostics(void) {
    console_writeln("VIOS Hardware Diagnostics");
    console_writeln("=========================");
    console_writeln("");

    /* CPU */
    const char *cpu = get_cpu_brand();
    console_write("CPU ......................... ");
    console_set_color(COLOR_GREEN, COLOR_BLACK);
    console_write("[ OK ]");
    console_reset_color();
    console_printf("  %s\n", cpu);

    /* Memory */
    uint64_t mem = get_total_memory();
    console_write("MEMORY ...................... ");
    if (mem > 0) {
        console_set_color(COLOR_GREEN, COLOR_BLACK);
        console_write("[ OK ]");
        console_reset_color();
        console_printf("  %u MiB detected\n", (unsigned)(mem / (1024 * 1024)));
    } else {
        console_set_color(COLOR_RED, COLOR_BLACK);
        console_write("[FAILED]");
        console_reset_color();
        console_writeln("  Unable to read memmap");
    }

    /* Framebuffer / GPU */
    console_write("GPU / FRAMEBUFFER ........... ");
    console_set_color(COLOR_GREEN, COLOR_BLACK);
    console_write("[ OK ]");
    console_reset_color();
    console_writeln("  Limine framebuffer active");

    /* HHDM */
    console_write("HHDM ........................ ");
    if (hhdm_request.response) {
        console_set_color(COLOR_GREEN, COLOR_BLACK);
        console_write("[ OK ]");
        console_reset_color();
        console_printf("  offset 0x%x\n", (unsigned)hhdm_request.response->offset);
    } else {
        console_set_color(COLOR_YELLOW, COLOR_BLACK);
        console_write("[ WARN ]");
        console_reset_color();
        console_writeln("  No HHDM response");
    }

    /* Executable address */
    console_write("KERNEL IMAGE ................ ");
    if (executable_address_request.response) {
        console_set_color(COLOR_GREEN, COLOR_BLACK);
        console_write("[ OK ]");
        console_reset_color();
        console_printf("  phys 0x%x\n", (unsigned)executable_address_request.response->physical_base);
    } else {
        console_set_color(COLOR_GREEN, COLOR_BLACK);
        console_write("[ OK ]");
        console_reset_color();
        console_writeln("  loaded");
    }

    /* Keyboard (assume PS/2 present on QEMU/x86, real check later) */
    console_write("KEYBOARD .................... ");
    console_set_color(COLOR_GREEN, COLOR_BLACK);
    console_write("[ OK ]");
    console_reset_color();
    console_writeln("  PS/2 / USB HID (basic)");

    /* USB Controller - not implemented yet */
    console_write("USB CONTROLLER .............. ");
    console_set_color(COLOR_YELLOW, COLOR_BLACK);
    console_write("[ SKIP ]");
    console_reset_color();
    console_writeln("  driver not loaded");

    /* Audio - not implemented */
    console_write("AUDIO ....................... ");
    console_set_color(COLOR_YELLOW, COLOR_BLACK);
    console_write("[ SKIP ]");
    console_reset_color();
    console_writeln("  no driver");

    /* Network - not implemented */
    console_write("NETWORK ..................... ");
    console_set_color(COLOR_YELLOW, COLOR_BLACK);
    console_write("[ SKIP ]");
    console_reset_color();
    console_writeln("  no driver");

    /* Storage */
    console_write("STORAGE ..................... ");
    console_set_color(COLOR_YELLOW, COLOR_BLACK);
    console_write("[ SKIP ]");
    console_reset_color();
    console_writeln("  RAM-only for now");

    /* Timer */
    console_write("TIMER ....................... ");
    console_set_color(COLOR_GREEN, COLOR_BLACK);
    console_write("[ OK ]");
    console_reset_color();
    console_writeln("  TSC / PIT stub");

    /* Interrupts */
    console_write("INTERRUPT CONTROLLER ........ ");
    console_set_color(COLOR_YELLOW, COLOR_BLACK);
    console_write("[ PARTIAL ]");
    console_reset_color();
    console_writeln("  IDT not fully initialized");

    /* Memory manager */
    console_write("MEMORY MANAGER .............. ");
    console_set_color(COLOR_YELLOW, COLOR_BLACK);
    console_write("[ PARTIAL ]");
    console_reset_color();
    console_writeln("  physical map only");

    console_writeln("");
    console_writeln("Diagnostics complete.");
    delay(80000000); /* ~1-2s depending on host */
}

/* ========== Stage 2: Loading bar ========== */
static void stage_loading(void) {
    console_clear();
    console_writeln("VIOS Kernel Initialization");
    console_writeln("");

    const char *steps[] = {
        "Setting up GDT stubs",
        "Preparing IDT",
        "Mapping physical memory",
        "Initializing heap allocator",
        "Setting up interrupt handlers",
        "Starting timer services",
        "Initializing scheduler stub",
        "Loading shell",
        "Mounting RAM filesystem",
        "Finalizing boot"
    };
    const int nsteps = 10;

    for (int i = 0; i <= 100; i++) {
        int bar_w = 40;
        int filled = (i * bar_w) / 100;
        console_write("\r[");
        for (int j = 0; j < bar_w; j++) {
            if (j < filled) console_putchar('=');
            else if (j == filled) console_putchar('>');
            else console_putchar(' ');
        }
        console_printf("] %d%%", i);
        if (i % 10 == 0 && i / 10 < nsteps) {
            console_printf("  %s", steps[i / 10]);
        }
        delay(1500000);
        uptime_ticks++;
    }
    console_writeln("");
    console_writeln("");
    console_writeln("Initialization complete.");
    delay(20000000);
}

/* ========== Stage 3: CRT effect ========== */
static void stage_crt(void) {
    /* Simple flicker / noise simulation */
    for (int i = 0; i < 8; i++) {
        console_set_color(COLOR_GREEN, COLOR_BLACK);
        console_clear();
        for (int y = 0; y < 20; y++) {
            for (int x = 0; x < 60; x++) {
                if ((x + y + i) % 7 == 0) console_putchar((char)(0x20 + ((x * y + i) % 90)));
                else console_putchar(' ');
            }
            console_putchar('\n');
        }
        delay(3000000);
        console_reset_color();
    }
    console_clear();
}

/* ========== Stage 4: Boot success ========== */
static void stage_boot_success(void) {
    console_clear();
    console_set_color(COLOR_GREEN, COLOR_BLACK);
    console_writeln("BOOTING SUCCESS .....");
    console_reset_color();
    delay(30000000);
    console_clear();
}

/* ========== Stage 5: VIOS MODEL connection ========== */
static void stage_model_connect(void) {
    console_clear();
    console_writeln("CONNECTING TO VIOS MODEL");
    console_writeln("");
    for (int i = 0; i < 3; i++) {
        console_write("........------");
        delay(15000000);
    }
    console_writeln("");
    console_writeln("");
    /* No real network stack yet -> honest OFFLINE */
    console_set_color(COLOR_YELLOW, COLOR_BLACK);
    console_writeln("OFFLINE");
    console_reset_color();
    console_writeln("(Network stack not implemented - no simulated success)");
    delay(25000000);
    console_clear();
}

/* ========== Dragon logo for kernel --version ========== */
static void print_dragon(void) {
    console_set_color(COLOR_CYAN, COLOR_BLACK);
    console_writeln("                  _______________");
    console_writeln("             ,===:'.,            `-._");
    console_writeln("                  `:.`---.__         `-._");
    console_writeln("                    `:.     `--.         `.");
    console_writeln("                      \\.        `.         `.");
    console_writeln("              (,,(,    \\.         `.   ____,-`.,");
    console_writeln("           (,'     `/   \\.   ,--.___`.'");
    console_writeln("       ,  ,'  ,--.  `,   \\.;'         `");
    console_writeln("        `{D, {    \\  :    \\;");
    console_writeln("          V,,'    /  /    //");
    console_writeln("          j;;    /  ,' ,-//.    ,---.      ,");
    console_writeln("          \\;'   /  ,' /  _  \\  /  _  \\   /`");
    console_writeln("                \\  `'  / \\  `'  / \\  `.' /");
    console_writeln("                 `.___,'   `.__,'   `.__,'");
    console_reset_color();
    console_writeln("");
}

void kernel_version(void) {
    print_dragon();
    console_printf("VIOS version      : %s\n", VIOS_VERSION);
    console_printf("Kernel version    : %s\n", VIOS_KERNEL_VERSION);
    console_printf("Architecture      : %s\n", VIOS_ARCH);
    console_printf("Kernel type       : %s\n", VIOS_KERNEL_TYPE);
    console_printf("Boot protocol     : %s\n", VIOS_BOOT_PROTOCOL);
    console_printf("Compiler          : %s\n", VIOS_COMPILER);
    console_printf("Build date        : %s %s\n", __DATE__, __TIME__);
    console_printf("CPU model         : %s\n", get_cpu_brand());
    uint64_t mem = get_total_memory();
    console_printf("RAM               : %u MiB\n", (unsigned)(mem / (1024 * 1024)));
    console_printf("Memory manager    : basic physical (Limine memmap)\n");
    console_printf("Filesystem        : RAMFS (stub)\n");
    console_printf("Uptime            : %u ticks\n", (unsigned)get_uptime_ticks());
}

/* ========== Simple shell ========== */
static int strcmp_simple(const char *a, const char *b); /* forward */

static char input_buf[256];
static int input_len = 0;

static void shell_prompt(void) {
    console_set_color(COLOR_GREEN, COLOR_BLACK);
    console_write("vios");
    console_reset_color();
    console_write("> ");
}

static void shell_help(void) {
    console_writeln("VIOS Shell - available commands:");
    console_writeln("  help              Show this help");
    console_writeln("  clear             Clear screen");
    console_writeln("  uname             System information");
    console_writeln("  uptime            Show uptime ticks");
    console_writeln("  free              Memory information");
    console_writeln("  kernel --version  Kernel logo and version info");
    console_writeln("  kernel --info     Short kernel info");
    console_writeln("  reboot            Halt (QEMU restart externally)");
    console_writeln("  poweroff          Halt CPU");
    console_writeln("  date              Placeholder date");
    console_writeln("");
    console_writeln("Note: Only commands listed above are implemented.");
}

static void shell_execute(const char *line) {
    if (line[0] == 0) return;

    if (strcmp_simple(line, "help") == 0) {
        shell_help();
    } else if (strcmp_simple(line, "clear") == 0) {
        console_clear();
    } else if (strcmp_simple(line, "uname") == 0) {
        console_printf("VIOS %s %s %s\n", VIOS_VERSION, VIOS_ARCH, VIOS_KERNEL_VERSION);
    } else if (strcmp_simple(line, "uptime") == 0) {
        console_printf("up %u ticks\n", (unsigned)get_uptime_ticks());
    } else if (strcmp_simple(line, "free") == 0) {
        uint64_t total = get_total_memory();
        uint64_t usable = get_usable_memory();
        console_printf("Total memory : %u MiB\n", (unsigned)(total / (1024 * 1024)));
        console_printf("Usable       : %u MiB\n", (unsigned)(usable / (1024 * 1024)));
    } else if (strcmp_simple(line, "kernel --version") == 0 ||
               strcmp_simple(line, "kernel -v") == 0) {
        kernel_version();
    } else if (strcmp_simple(line, "kernel --info") == 0) {
        console_printf("VIOS Kernel %s (%s)\n", VIOS_KERNEL_VERSION, VIOS_ARCH);
    } else if (strcmp_simple(line, "reboot") == 0 || strcmp_simple(line, "poweroff") == 0) {
        console_writeln("System halted. Use QEMU monitor or host to restart.");
        hcf();
    } else if (strcmp_simple(line, "date") == 0) {
        console_writeln("Date/time services not implemented yet.");
    } else {
        console_printf("Unknown command: %s\n", line);
        console_writeln("Type 'help' for available commands.");
    }
}

/* Minimal string helpers (freestanding) */
static int strcmp_simple(const char *a, const char *b) {
    while (*a && *b && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

/* Keyboard input via PS/2 (polling, QEMU friendly) */
static uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static int kb_has_key(void) {
    return (inb(0x64) & 1) != 0;
}

static char scancode_to_ascii(uint8_t sc) {
    static const char map[128] = {
        0, 27, '1','2','3','4','5','6','7','8','9','0','-','=', '\b',
        '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n', 0,
        'a','s','d','f','g','h','j','k','l',';','\'','`', 0, '\\',
        'z','x','c','v','b','n','m',',','.','/', 0, '*', 0, ' ',
    };
    if (sc < 128) return map[sc];
    return 0;
}

static void shell_loop(void) {
    console_writeln("Welcome to VIOS Shell");
    console_writeln("Type 'help' for commands. Type 'kernel --version' for logo.");
    console_writeln("");
    shell_prompt();
    input_len = 0;

    for (;;) {
        uptime_ticks++;
        if (kb_has_key()) {
            uint8_t sc = inb(0x60);
            if (sc & 0x80) continue; /* key release */
            char c = scancode_to_ascii(sc);
            if (c == '\n' || c == '\r') {
                console_putchar('\n');
                input_buf[input_len] = 0;
                shell_execute(input_buf);
                input_len = 0;
                shell_prompt();
            } else if (c == '\b') {
                if (input_len > 0) {
                    input_len--;
                    /* simple backspace visual */
                    console_putchar('\b');
                    console_putchar(' ');
                    console_putchar('\b');
                }
            } else if (c && input_len < (int)sizeof(input_buf) - 1) {
                input_buf[input_len++] = c;
                console_putchar(c);
            }
        }
        /* small idle */
        __asm__ volatile ("pause");
    }
}

/* ========== Kernel entry ========== */
void kmain(void) {
    /* Verify Limine base revision */
    if (LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision) == false) {
        hcf();
    }

    console_init();
    if (framebuffer_request.response == NULL) {
        /* No FB - nothing we can do visually */
        hcf();
    }

    /* Boot stages as specified */
    stage_diagnostics();
    stage_loading();
    stage_crt();
    stage_boot_success();
    stage_model_connect();

    /* Main menu / shell */
    shell_loop();

    hcf();
}
