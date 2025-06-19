////////////////////////////////////////////////////////////////////////////////
//  change these a little bit for different behavior
//
////////////////////////////////////////////////////////////////////////////////
#include "stdlib.h"
#include <attrib.h>
#include <boot/custom.h>
#include <drivers/display.h>
#include <drivers/timer.h>
#include <utils.h>
// #include <resources/zorzal.h>
#include <drivers/irq.h>
#include <stdio.h>
#include <sys/_intsup.h>
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

unsigned long get_cnt()
{
	unsigned long cnt;
	asm("mrs %0, CNTPCT_EL0" : "=r"(cnt));
	return cnt;
}

__attribute__((unused)) static void timer_compare()
{
	timer_init();

	unsigned long last_system = 0, count, system, last_count = 0;
	for (;;)
	{
		timer_count(&count, &system);

		printf(
			"Count: %lu\tCurrent system timer: %lu\tDiff system timer: %ld\n",
			count,
			system,
			(system - last_system) / (count - last_count)
		);
		last_system = system;
		last_count = count;
		// next += 1000;
		asm("wfi");
	}
}

__attribute__((unused)) static void generic_timer_test()
{
	// Clear IMASK (bit 1) in CNTP_CTL_EL0 and get the frequency of the counter
	unsigned long freq, interval;
	asm("msr CNTP_CTL_EL0, %1\n"
		"mrs %0, CNTFRQ_EL0\n"
		: "=r"(freq)
		: "r"(0));
	mreg32(CORE0_INT_CTR) = 1 << 1; // Enable IRQ Core 0
	interval = 250 * freq / 1000;
	unsigned long last_system = 0, system;
	for (;;)
	{
		asm("msr CNTP_TVAL_EL0, %2\n"
			"msr CNTP_CTL_EL0, %3\n"
			"isb\n"
			"mrs %0, CNTPCT_EL0\n"
			"wfi\n"
			"mrs %1, CNTPCT_EL0\n"
			: "=r"(last_system), "=r"(system)
			: "r"(interval), "r"(1)); // 250ms

		printf(
			"Current system timer: %lu\tDiff system timer (us): %ld\n", system, (system - last_system) * 1000000 / freq
		);
		last_system = system;
		// last_count = count;
		// next += 1000;
	}
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

	generic_timer_test();
	return 0;
}