#ifndef VIOS_KERNEL_H
#define VIOS_KERNEL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define VIOS_VERSION        "0.1.0"
#define VIOS_KERNEL_VERSION "0.1.0-dev"
#define VIOS_ARCH           "x86-64"
#define VIOS_KERNEL_TYPE    "monolithic (freestanding)"
#define VIOS_BOOT_PROTOCOL  "Limine"
#define VIOS_COMPILER       "GCC (cross freestanding)"

/* Halt and catch fire */
__attribute__((noreturn)) void hcf(void);

/* Memory info from Limine */
uint64_t get_total_memory(void);
uint64_t get_usable_memory(void);
const char *get_cpu_brand(void);

/* Uptime in ticks (rough) */
uint64_t get_uptime_ticks(void);

#endif
