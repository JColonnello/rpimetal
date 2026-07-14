#include <drivers/simple-uart.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/printf.h>

static void _putc(void *p, char c)
{
	uart_send(c);
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
