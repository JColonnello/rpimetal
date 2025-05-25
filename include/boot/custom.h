#pragma once

#include <boot.h>

struct boot_customdata
{
	void (*test_function)(int, int*);
	int *module_data;
};
