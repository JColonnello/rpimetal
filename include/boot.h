#pragma once

// Struct containing boot info for the kernel
struct boot_info
{
	void *boot_memory_end;
	void *memory_start;
	void *memory_end;
};

union boot_userdata
{
	struct boot_customdata *custom;
	void *ptr;
};

// The type for the expected _start function of the kernel
typedef void (*start_function_type)(struct boot_info *info, union boot_userdata userdata);
