////////////////////////////////////////////////////////////////////////////////
//  change these a little bit for different behavior
//
////////////////////////////////////////////////////////////////////////////////
#include "stdlib.h"
#include <attrib.h>
#include <boot/custom.h>
#include <drivers/display.h>
#include <drivers/timer.h>
#include <drivers/uart.h>
#include <utils.h>
// #include <resources/zorzal.h>
#include <drivers/irq.h>
#include <stdio.h>
#include <sys/reent.h>

//// callbacks

// a function to be used as callback
int my_callback_01(int a)
{
	// printf("my_callback_01 called!\n");
	return a * 2;
}

int my_callback_02(int a)
{
	// printf("my_callback_02 called!\n");
	return a * 4;
}

__thread int tls_int = 3, *module_data_ptr;
int my_callback_03(int a)
{
	// printf("my_callback_03 called! Using %p\n", module_data_ptr);
	return a * *module_data_ptr;
}

typedef int (*t_callback)(int);
typedef void (*t_test_function)(int, int *);

// test_unit.o is expected to call a function with the name "callback";
// we will relocate those calls to the address in the my_callback variable
alias(my_callback_03, callback);
// our job is to load the binary code of the object file into memory,
// then find the address of the function with the following name
const char *test_function_name = "test_function_02";
// store it on the following pointer:
// and then execute it on the two arguments "in" and "out":
int in = 10;
int out[4];

// this pointer will eventually store the address of the function in test_unit.o with the name test_function_name
t_test_function test_function;

destructor static void print_exit()
{
	printf("Exitting kernel\n");
}

static void timer_callback(unsigned timer, void *data)
{
	const char *text = data;
	printf("%s current time: %lu us\n", text, timer_monotonic());
}

static volatile bool uart_available = false;
static void uart_callback(size_t available)
{
	uart_available = available > 0;
}

int kernel_start(struct boot_info *info, union boot_userdata userdata)
{
	struct boot_customdata *data = userdata.custom;
	stdlib_set_mem_limits(info->memory_start, info->memory_end);
	module_data_ptr = data->module_data;
	void (*test)(int, int *) = data->test_function;

	test(in, out);
	printf("out = { %d, %d, %d, %d }\n", out[0], out[1], out[2], out[3]);
	puts("Done!\n");

	// puts("Starting display driver...\n");
	// lfb_init();
	// lfb_showpicture(header_data, height, width);

	uart_set_rx_callback(uart_callback);
	// Wait 200ms and print current time
	timer_register(1000000, true, timer_callback, "Callback");
	for (;;)
	{
		timer_millisleep(2000);
		unsigned long time = timer_monotonic();
		printf("Sleep current time: %lu us\n", time);
		static char s[128];
		if (uart_available)
		{
			size_t n = uart_recv_buffer(s, sizeof(s));
			uart_available = false; // reset the flag
			printf("Received %lu bytes: %.*s\n", n, (int)n, s);
		}
	}

	return 0;
}