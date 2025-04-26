#include <stdint.h>
#include <stdio.h>
#include "irq.h"
#include "peripherals/uart.h"
#include "entry.h"
#include "utils.h"
#include "peripherals/irq.h"
#include "peripherals/timer.h"
#include "loader.h"

////////////////////////////////////////////////////////////////////////////////
//  change these a little bit for different behavior
//
////////////////////////////////////////////////////////////////////////////////

//// callbacks

// a function to be used as callback
int my_callback_01(int a)
{
	printf("my_callback_01 called!\n");
	return a * 2;
}

int my_callback_02(int a)
{
	printf("my_callback_02 called!\n");
	return a * 4;
}

__thread int tls_int = 3, *module_data_ptr;
int my_callback_03(int a)
{
	printf("my_callback_03 called! Using %p\n", module_data_ptr);
	return a * *module_data_ptr;
}

typedef int (*t_callback)(int);
typedef void (*t_test_function)(int, int *);

// test_unit.o is expected to call a function with the name "callback";
// we will relocate those calls to the address in the my_callback variable
t_callback callback = my_callback_03;
// our job is to load the binary code of the object file into memory,
// then find the address of the function with the following name
const char *test_function_name = "test_function_02";
// store it on the following pointer:
// and then execute it on the two arguments "in" and "out":
int in = 10;
int out[4];

// this pointer will eventually store the address of the function in test_unit.o with the name test_function_name
t_test_function test_function;
FILE_FROM_SYMBOL_FUNC_DECL(test_unit);

int main(void)
{
	irq_vector_init();
	// timer_init();
	enable_interrupt_controller();
	enable_irq();
	uart_init();

	// Exception test
    // unsigned int r=*((volatile unsigned int*)0xFFFFFFFFFF000000);
	// r++;

	loader_init();
	symbol_data sym[] = 
	{
		{ .name = "callback", .address = callback },
		{ .name = "tls_int", .address = local_tls_offset(&tls_int) },
	};
	loader_add_starting_symbols(sizeof(sym)/sizeof(*sym), sym);
	loader_load_file(FILE_FROM_SYMBOL_FUNC_CALL(test_unit), "build/modules/test_unit.ko");
	loader_print_tls_layout(tls_schema);
    struct tls_data *tcb = loader_create_tcb();
    loader_switch_tcb(tcb);
	// once everything is patched, we should be able to run test_function
	// which should call our callback!
	test_function = loader_search_symbol(test_function_name);
    module_data_ptr = loader_tls_ptr(tcb, (ssize_t)loader_search_symbol("module_data"));
	
	printf("calling \"%s\" from test_unit.o on %d, and \"out\".\n", test_function_name, in);
	test_function(in, out);
	printf("out = { %d, %d, %d, %d }\n", out[0], out[1], out[2], out[3]);
	printf("Done!\n");
	return 0;
}
