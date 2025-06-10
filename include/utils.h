#pragma once
#include <stdint.h>

// extern void delay ( unsigned long);
#define mreg32(ptr) (*(volatile uint32_t *)(ptr))
// extern int get_el ( void );
