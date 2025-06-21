#pragma once
#include <stdint.h>

// extern void delay ( unsigned long);
#define mreg32(ptr) (*(volatile uint32_t *)(ptr))
// extern int get_el ( void );

__attribute__((always_inline)) unsigned char static inline get_core_id()
{
	unsigned char mpidr;
	asm volatile("mrs %0, mpidr_el1" : "=r"(mpidr));
	return mpidr;
}
