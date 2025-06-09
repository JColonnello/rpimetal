#include "attrib.h"
#include <drivers/uart.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/stat.h>

int _fstat(int fildes, struct stat *buf)
{
	if (fildes >= 0 && fildes <= 2)
	{
		*buf = (struct stat){
		    .st_mode = S_IFCHR,
		};
		return 0;
	}
	errno = EBADF;
	return -1;
}

static void *mem_start, *mem_end;
void stdlib_set_mem_limits(void *start, void *end)
{
	mem_start = start;
	mem_end = end;
}

void *_sbrk(intptr_t increment)
{
	static void *curr_break;
	if (curr_break < mem_start)
		curr_break = mem_start;

	if (increment == 0)
		return curr_break;

	void *desired = curr_break + increment, *last_break = curr_break;
	if (desired > mem_end || desired < mem_start)
	{
		errno = ENOMEM;
		return (void *)-1;
	}

	curr_break += increment;
	return last_break;
}

int _isatty(int fd)
{
	if (fd >= 0 && fd <= 2)
		return true;
	else
		return false;
}

int _write(int fd, const void *buf, size_t count)
{
	uart_send_buffer(buf, count);
	return count;
}

int _close(int fd)
{
	if (fd >= 0 && fd <= 2)
		return 0;
	else
	{
		errno = EBADF;
		return -1;
	}
}

noreturn void halt()
{
	// Loop forever
	for (;;)
		// Wait for interrupt
		asm volatile("wfi");
}
noreturn weak alias(halt, proc_hang);

void _exit(int status)
{
	halt();
}
