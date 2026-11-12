#ifndef VIOS_CONSOLE_H
#define VIOS_CONSOLE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

void console_init(void);
void console_clear(void);
void console_putchar(char c);
void console_write(const char *s);
void console_writeln(const char *s);
void console_printf(const char *fmt, ...);
void console_set_color(uint32_t fg, uint32_t bg);
void console_reset_color(void);

/* Colors (ARGB) */
#define COLOR_BLACK   0xFF000000
#define COLOR_WHITE   0xFFFFFFFF
#define COLOR_GREEN   0xFF00FF00
#define COLOR_RED     0xFFFF0000
#define COLOR_YELLOW  0xFFFFFF00
#define COLOR_CYAN    0xFF00FFFF
#define COLOR_BLUE    0xFF0000FF
#define COLOR_GRAY    0xFFAAAAAA
#define COLOR_DARK    0xFF222222

#endif
