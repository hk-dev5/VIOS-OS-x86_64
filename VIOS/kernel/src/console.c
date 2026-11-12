#include "console.h"
#include "kernel.h"
#include <stdarg.h>
#include <limine.h>

/* Limine framebuffer request */
__attribute__((used, section(".limine_requests")))
volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0
};

static struct limine_framebuffer *fb = NULL;
static uint32_t *fb_ptr = NULL;
static uint64_t fb_width = 0;
static uint64_t fb_height = 0;
static uint64_t fb_pitch = 0;
static uint16_t fb_bpp = 0;

static uint32_t cursor_x = 0;
static uint32_t cursor_y = 0;
static uint32_t fg_color = COLOR_WHITE;
static uint32_t bg_color = COLOR_BLACK;

/* Simple 8x16 font (partial, enough for basic terminal) */
static const uint8_t font8x16[128][16] = {
    /* 0-31 control - empty */
    [0 ... 31] = {0},
    /* space */
    [' '] = {0},
    /* ! */
    ['!'] = {0x18,0x18,0x18,0x18,0x18,0x18,0x00,0x18,0x18,0,0,0,0,0,0,0},
    /* " */
    ['"'] = {0x36,0x36,0x36,0x00,0,0,0,0,0,0,0,0,0,0,0,0},
    /* # */
    ['#'] = {0x36,0x36,0x7F,0x36,0x36,0x7F,0x36,0x36,0,0,0,0,0,0,0,0},
    /* $ */
    ['$'] = {0x0C,0x3E,0x6C,0x38,0x1C,0x6C,0x3E,0x18,0,0,0,0,0,0,0,0},
    /* % */
    ['%'] = {0x63,0x66,0x0C,0x18,0x30,0x66,0xC6,0,0,0,0,0,0,0,0,0},
    /* & */
    ['&'] = {0x1C,0x36,0x1C,0x38,0x6F,0x66,0x3B,0,0,0,0,0,0,0,0,0},
    /* ' */
    ['\''] = {0x18,0x18,0x18,0,0,0,0,0,0,0,0,0,0,0,0,0},
    /* ( */
    ['('] = {0x0C,0x18,0x30,0x30,0x30,0x18,0x0C,0,0,0,0,0,0,0,0,0},
    /* ) */
    [')'] = {0x30,0x18,0x0C,0x0C,0x0C,0x18,0x30,0,0,0,0,0,0,0,0,0},
    /* * */
    ['*'] = {0x00,0x66,0x3C,0xFF,0x3C,0x66,0,0,0,0,0,0,0,0,0,0},
    /* + */
    ['+'] = {0x00,0x18,0x18,0x7E,0x18,0x18,0,0,0,0,0,0,0,0,0,0},
    /* , */
    [','] = {0,0,0,0,0,0x18,0x18,0x30,0,0,0,0,0,0,0,0},
    /* - */
    ['-'] = {0,0,0,0x7E,0,0,0,0,0,0,0,0,0,0,0,0},
    /* . */
    ['.'] = {0,0,0,0,0,0x18,0x18,0,0,0,0,0,0,0,0,0},
    /* / */
    ['/'] = {0x03,0x06,0x0C,0x18,0x30,0x60,0xC0,0,0,0,0,0,0,0,0,0},
    /* 0-9 */
    ['0'] = {0x3C,0x66,0x6E,0x76,0x66,0x66,0x3C,0,0,0,0,0,0,0,0,0},
    ['1'] = {0x18,0x38,0x18,0x18,0x18,0x18,0x7E,0,0,0,0,0,0,0,0,0},
    ['2'] = {0x3C,0x66,0x06,0x0C,0x18,0x30,0x7E,0,0,0,0,0,0,0,0,0},
    ['3'] = {0x3C,0x66,0x06,0x1C,0x06,0x66,0x3C,0,0,0,0,0,0,0,0,0},
    ['4'] = {0x0C,0x1C,0x3C,0x6C,0x7E,0x0C,0x0C,0,0,0,0,0,0,0,0,0},
    ['5'] = {0x7E,0x60,0x7C,0x06,0x06,0x66,0x3C,0,0,0,0,0,0,0,0,0},
    ['6'] = {0x1C,0x30,0x60,0x7C,0x66,0x66,0x3C,0,0,0,0,0,0,0,0,0},
    ['7'] = {0x7E,0x06,0x0C,0x18,0x30,0x30,0x30,0,0,0,0,0,0,0,0,0},
    ['8'] = {0x3C,0x66,0x66,0x3C,0x66,0x66,0x3C,0,0,0,0,0,0,0,0,0},
    ['9'] = {0x3C,0x66,0x66,0x3E,0x06,0x0C,0x38,0,0,0,0,0,0,0,0,0},
    /* : */
    [':'] = {0,0x18,0x18,0,0,0x18,0x18,0,0,0,0,0,0,0,0,0},
    /* ; */
    [';'] = {0,0x18,0x18,0,0,0x18,0x18,0x30,0,0,0,0,0,0,0,0},
    /* < */
    ['<'] = {0x0C,0x18,0x30,0x60,0x30,0x18,0x0C,0,0,0,0,0,0,0,0,0},
    /* = */
    ['='] = {0,0,0x7E,0,0x7E,0,0,0,0,0,0,0,0,0,0,0},
    /* > */
    ['>'] = {0x60,0x30,0x18,0x0C,0x18,0x30,0x60,0,0,0,0,0,0,0,0,0},
    /* ? */
    ['?'] = {0x3C,0x66,0x06,0x0C,0x18,0x00,0x18,0x18,0,0,0,0,0,0,0,0},
    /* @ */
    ['@'] = {0x3C,0x66,0x6E,0x6A,0x6E,0x60,0x3C,0,0,0,0,0,0,0,0,0},
    /* A-Z */
    ['A'] = {0x18,0x3C,0x66,0x66,0x7E,0x66,0x66,0,0,0,0,0,0,0,0,0},
    ['B'] = {0x7C,0x66,0x66,0x7C,0x66,0x66,0x7C,0,0,0,0,0,0,0,0,0},
    ['C'] = {0x3C,0x66,0x60,0x60,0x60,0x66,0x3C,0,0,0,0,0,0,0,0,0},
    ['D'] = {0x78,0x6C,0x66,0x66,0x66,0x6C,0x78,0,0,0,0,0,0,0,0,0},
    ['E'] = {0x7E,0x60,0x60,0x7C,0x60,0x60,0x7E,0,0,0,0,0,0,0,0,0},
    ['F'] = {0x7E,0x60,0x60,0x7C,0x60,0x60,0x60,0,0,0,0,0,0,0,0,0},
    ['G'] = {0x3C,0x66,0x60,0x6E,0x66,0x66,0x3C,0,0,0,0,0,0,0,0,0},
    ['H'] = {0x66,0x66,0x66,0x7E,0x66,0x66,0x66,0,0,0,0,0,0,0,0,0},
    ['I'] = {0x3C,0x18,0x18,0x18,0x18,0x18,0x3C,0,0,0,0,0,0,0,0,0},
    ['J'] = {0x1E,0x0C,0x0C,0x0C,0x0C,0x6C,0x38,0,0,0,0,0,0,0,0,0},
    ['K'] = {0x66,0x6C,0x78,0x70,0x78,0x6C,0x66,0,0,0,0,0,0,0,0,0},
    ['L'] = {0x60,0x60,0x60,0x60,0x60,0x60,0x7E,0,0,0,0,0,0,0,0,0},
    ['M'] = {0x63,0x77,0x7F,0x6B,0x63,0x63,0x63,0,0,0,0,0,0,0,0,0},
    ['N'] = {0x66,0x76,0x7E,0x7E,0x6E,0x66,0x66,0,0,0,0,0,0,0,0,0},
    ['O'] = {0x3C,0x66,0x66,0x66,0x66,0x66,0x3C,0,0,0,0,0,0,0,0,0},
    ['P'] = {0x7C,0x66,0x66,0x7C,0x60,0x60,0x60,0,0,0,0,0,0,0,0,0},
    ['Q'] = {0x3C,0x66,0x66,0x66,0x6A,0x6C,0x36,0,0,0,0,0,0,0,0,0},
    ['R'] = {0x7C,0x66,0x66,0x7C,0x6C,0x66,0x66,0,0,0,0,0,0,0,0,0},
    ['S'] = {0x3C,0x66,0x60,0x3C,0x06,0x66,0x3C,0,0,0,0,0,0,0,0,0},
    ['T'] = {0x7E,0x18,0x18,0x18,0x18,0x18,0x18,0,0,0,0,0,0,0,0,0},
    ['U'] = {0x66,0x66,0x66,0x66,0x66,0x66,0x3C,0,0,0,0,0,0,0,0,0},
    ['V'] = {0x66,0x66,0x66,0x66,0x66,0x3C,0x18,0,0,0,0,0,0,0,0,0},
    ['W'] = {0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0,0,0,0,0,0,0,0,0},
    ['X'] = {0x66,0x66,0x3C,0x18,0x3C,0x66,0x66,0,0,0,0,0,0,0,0,0},
    ['Y'] = {0x66,0x66,0x66,0x3C,0x18,0x18,0x18,0,0,0,0,0,0,0,0,0},
    ['Z'] = {0x7E,0x06,0x0C,0x18,0x30,0x60,0x7E,0,0,0,0,0,0,0,0,0},
    /* [ \ ] ^ _ ` */
    ['['] = {0x3C,0x30,0x30,0x30,0x30,0x30,0x3C,0,0,0,0,0,0,0,0,0},
    ['\\'] = {0xC0,0x60,0x30,0x18,0x0C,0x06,0x03,0,0,0,0,0,0,0,0,0},
    [']'] = {0x3C,0x0C,0x0C,0x0C,0x0C,0x0C,0x3C,0,0,0,0,0,0,0,0,0},
    ['^'] = {0x18,0x3C,0x66,0,0,0,0,0,0,0,0,0,0,0,0,0},
    ['_'] = {0,0,0,0,0,0,0,0xFF,0,0,0,0,0,0,0,0},
    ['`'] = {0x30,0x18,0x0C,0,0,0,0,0,0,0,0,0,0,0,0,0},
    /* a-z (simplified, reuse uppercase style for minimalism) */
    ['a'] = {0,0,0x3C,0x06,0x3E,0x66,0x3E,0,0,0,0,0,0,0,0,0},
    ['b'] = {0x60,0x60,0x7C,0x66,0x66,0x66,0x7C,0,0,0,0,0,0,0,0,0},
    ['c'] = {0,0,0x3C,0x66,0x60,0x66,0x3C,0,0,0,0,0,0,0,0,0},
    ['d'] = {0x06,0x06,0x3E,0x66,0x66,0x66,0x3E,0,0,0,0,0,0,0,0,0},
    ['e'] = {0,0,0x3C,0x66,0x7E,0x60,0x3C,0,0,0,0,0,0,0,0,0},
    ['f'] = {0x1C,0x30,0x7C,0x30,0x30,0x30,0x30,0,0,0,0,0,0,0,0,0},
    ['g'] = {0,0,0x3E,0x66,0x66,0x3E,0x06,0x3C,0,0,0,0,0,0,0,0},
    ['h'] = {0x60,0x60,0x7C,0x66,0x66,0x66,0x66,0,0,0,0,0,0,0,0,0},
    ['i'] = {0x18,0,0x38,0x18,0x18,0x18,0x3C,0,0,0,0,0,0,0,0,0},
    ['j'] = {0x0C,0,0x1C,0x0C,0x0C,0x0C,0x6C,0x38,0,0,0,0,0,0,0,0},
    ['k'] = {0x60,0x60,0x66,0x6C,0x78,0x6C,0x66,0,0,0,0,0,0,0,0,0},
    ['l'] = {0x38,0x18,0x18,0x18,0x18,0x18,0x3C,0,0,0,0,0,0,0,0,0},
    ['m'] = {0,0,0x66,0x7F,0x7F,0x6B,0x63,0,0,0,0,0,0,0,0,0},
    ['n'] = {0,0,0x7C,0x66,0x66,0x66,0x66,0,0,0,0,0,0,0,0,0},
    ['o'] = {0,0,0x3C,0x66,0x66,0x66,0x3C,0,0,0,0,0,0,0,0,0},
    ['p'] = {0,0,0x7C,0x66,0x66,0x7C,0x60,0x60,0,0,0,0,0,0,0,0},
    ['q'] = {0,0,0x3E,0x66,0x66,0x3E,0x06,0x06,0,0,0,0,0,0,0,0},
    ['r'] = {0,0,0x6C,0x76,0x60,0x60,0x60,0,0,0,0,0,0,0,0,0},
    ['s'] = {0,0,0x3E,0x60,0x3C,0x06,0x7C,0,0,0,0,0,0,0,0,0},
    ['t'] = {0x30,0x30,0x7C,0x30,0x30,0x30,0x1C,0,0,0,0,0,0,0,0,0},
    ['u'] = {0,0,0x66,0x66,0x66,0x66,0x3E,0,0,0,0,0,0,0,0,0},
    ['v'] = {0,0,0x66,0x66,0x66,0x3C,0x18,0,0,0,0,0,0,0,0,0},
    ['w'] = {0,0,0x63,0x6B,0x7F,0x3E,0x36,0,0,0,0,0,0,0,0,0},
    ['x'] = {0,0,0x66,0x3C,0x18,0x3C,0x66,0,0,0,0,0,0,0,0,0},
    ['y'] = {0,0,0x66,0x66,0x66,0x3E,0x06,0x3C,0,0,0,0,0,0,0,0},
    ['z'] = {0,0,0x7E,0x0C,0x18,0x30,0x7E,0,0,0,0,0,0,0,0,0},
    /* { | } ~ */
    ['{'] = {0x0E,0x18,0x18,0x70,0x18,0x18,0x0E,0,0,0,0,0,0,0,0,0},
    ['|'] = {0x18,0x18,0x18,0x18,0x18,0x18,0x18,0,0,0,0,0,0,0,0,0},
    ['}'] = {0x70,0x18,0x18,0x0E,0x18,0x18,0x70,0,0,0,0,0,0,0,0,0},
    ['~'] = {0x3B,0x6E,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
};

static void draw_char(uint32_t x, uint32_t y, char c, uint32_t fg, uint32_t bg) {
    if (!fb_ptr) return;
    if ((unsigned char)c >= 128) c = '?';
    const uint8_t *glyph = font8x16[(unsigned char)c];
    for (int row = 0; row < 16; row++) {
        uint8_t bits = glyph[row];
        for (int col = 0; col < 8; col++) {
            uint32_t px = x + col;
            uint32_t py = y + row;
            if (px >= fb_width || py >= fb_height) continue;
            uint32_t color = (bits & (0x80 >> col)) ? fg : bg;
            fb_ptr[py * (fb_pitch / 4) + px] = color;
        }
    }
}

static void scroll(void) {
    if (!fb_ptr) return;
    uint32_t line_height = 16;
    /* Move rows up */
    for (uint64_t y = line_height; y < fb_height; y++) {
        for (uint64_t x = 0; x < fb_width; x++) {
            fb_ptr[(y - line_height) * (fb_pitch / 4) + x] =
                fb_ptr[y * (fb_pitch / 4) + x];
        }
    }
    /* Clear bottom */
    for (uint64_t y = fb_height - line_height; y < fb_height; y++) {
        for (uint64_t x = 0; x < fb_width; x++) {
            fb_ptr[y * (fb_pitch / 4) + x] = bg_color;
        }
    }
    cursor_y -= line_height;
}

void console_init(void) {
    if (framebuffer_request.response == NULL ||
        framebuffer_request.response->framebuffer_count < 1) {
        return;
    }
    fb = framebuffer_request.response->framebuffers[0];
    fb_ptr = (uint32_t *)fb->address;
    fb_width = fb->width;
    fb_height = fb->height;
    fb_pitch = fb->pitch;
    fb_bpp = fb->bpp;
    cursor_x = 0;
    cursor_y = 0;
    console_clear();
}

void console_clear(void) {
    if (!fb_ptr) return;
    for (uint64_t i = 0; i < (fb_pitch / 4) * fb_height; i++) {
        fb_ptr[i] = bg_color;
    }
    cursor_x = 0;
    cursor_y = 0;
}

void console_set_color(uint32_t fg, uint32_t bg) {
    fg_color = fg;
    bg_color = bg;
}

void console_reset_color(void) {
    fg_color = COLOR_WHITE;
    bg_color = COLOR_BLACK;
}

void console_putchar(char c) {
    if (!fb_ptr) return;
    if (c == '\n') {
        cursor_x = 0;
        cursor_y += 16;
        if (cursor_y + 16 > fb_height) scroll();
        return;
    }
    if (c == '\r') {
        cursor_x = 0;
        return;
    }
    if (c == '\t') {
        cursor_x = (cursor_x + 32) & ~31;
        if (cursor_x + 8 > fb_width) {
            cursor_x = 0;
            cursor_y += 16;
            if (cursor_y + 16 > fb_height) scroll();
        }
        return;
    }
    draw_char(cursor_x, cursor_y, c, fg_color, bg_color);
    cursor_x += 8;
    if (cursor_x + 8 > fb_width) {
        cursor_x = 0;
        cursor_y += 16;
        if (cursor_y + 16 > fb_height) scroll();
    }
}

void console_write(const char *s) {
    if (!s) return;
    while (*s) {
        console_putchar(*s++);
    }
}

void console_writeln(const char *s) {
    console_write(s);
    console_putchar('\n');
}

/* Minimal printf supporting %s %d %u %x %c %% */
static void print_uint(uint64_t n, int base, bool upper) {
    char buf[32];
    int i = 0;
    if (n == 0) {
        console_putchar('0');
        return;
    }
    while (n) {
        int d = n % base;
        buf[i++] = (d < 10) ? ('0' + d) : (upper ? 'A' : 'a') + (d - 10);
        n /= base;
    }
    while (i--) console_putchar(buf[i]);
}

void console_printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    for (const char *p = fmt; *p; p++) {
        if (*p != '%') {
            console_putchar(*p);
            continue;
        }
        p++;
        switch (*p) {
            case 's': {
                const char *s = va_arg(args, const char *);
                console_write(s ? s : "(null)");
                break;
            }
            case 'd': {
                int64_t n = va_arg(args, int64_t);
                if (n < 0) {
                    console_putchar('-');
                    n = -n;
                }
                print_uint((uint64_t)n, 10, false);
                break;
            }
            case 'u': {
                print_uint(va_arg(args, uint64_t), 10, false);
                break;
            }
            case 'x': {
                print_uint(va_arg(args, uint64_t), 16, false);
                break;
            }
            case 'X': {
                print_uint(va_arg(args, uint64_t), 16, true);
                break;
            }
            case 'c': {
                console_putchar((char)va_arg(args, int));
                break;
            }
            case '%':
                console_putchar('%');
                break;
            default:
                console_putchar('%');
                console_putchar(*p);
                break;
        }
    }
    va_end(args);
}
