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

static void uart_receive_callback(char c)
{
	// Echo received character
	printf("%x", c);
}

int kernel_start()
{
	init_printf(0, _putc);
	printf("Hello, RPi Metal!\n");
	uart_set_rx_callback(uart_receive_callback);
	// Exception test
	// unsigned int r=*((volatile unsigned int*)0xFFFFFFFFFF000000);
	// r++;
	for (;;)
		asm("wfi");

	return 0;
}
