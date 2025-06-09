#pragma once
#include <stdbool.h>

bool mmu_is_enabled();
static inline void mmu_set_t0el1(void *table)
{
	asm volatile("msr ttbr0_el1, %0" : : "r"(table));
}
