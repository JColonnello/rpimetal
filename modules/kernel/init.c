/*
 * Copyright (C) 2004 CodeSourcery, LLC
 *
 * Permission to use, copy, modify, and distribute this file
 * for any purpose is hereby granted without fee, provided that
 * the above copyright notice and this notice appears in all
 * copies.
 *
 * This file is distributed WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */

/* Handle ELF .{pre_init,init,fini}_array sections.  */
#include <attrib.h>
#include <boot.h>
#include <sys/types.h>

/* These magic symbols are provided by the linker.  */
weak extern void (*__preinit_array_start[])(void);
weak extern void (*__preinit_array_end[])(void);
weak extern void (*__init_array_start[])(void);
weak extern void (*__init_array_end[])(void);

weak void _init(struct boot_info *info, union boot_userdata userdata)
{
}

/* Iterate over all the init routines.  */
void libc_init_array(struct boot_info *info, union boot_userdata userdata)
{
	size_t count;
	size_t i;

	count = __preinit_array_end - __preinit_array_start;
	for (i = 0; i < count; i++)
		__preinit_array_start[i]();

	_init(info, userdata);

	count = __init_array_end - __init_array_start;
	for (i = 0; i < count; i++)
		__init_array_start[i]();
}
