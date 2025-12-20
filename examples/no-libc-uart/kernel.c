#include <drivers/simple-uart.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/printf.h>

static void _putc(void *p, char c)
{
	uart_send(c);
}

// Adhoc definition of memcpy
void *memcpy(void *dest, const void *src, size_t n)
{
	char *place = (char *)dest;
	const char *handler = (const char *)src;
	for (size_t i = 0; i < n; i++)
		place[i] = handler[i];
	return dest;
}

int main(void)
{
	init_printf(0, _putc);
	printf("Hello, RPi Metal!\n");
	// Exception test
	// unsigned int r=*((volatile unsigned int*)0xFFFFFFFFFF000000);
	// r++;

	exit(1);
	return 0;
}
