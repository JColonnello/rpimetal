#include <stdint.h>
#include <stdio.h>
#include "irq.h"
#include <drivers/uart.h>
#include "entry.h"
#include "utils.h"
#include <drivers/irq.h>
#include <drivers/timer.h>
#include "loader.h"

FILE_FROM_SYMBOL_FUNC_DECL(testing_test);
FILE_FROM_SYMBOL_FUNC_DECL(kernel);

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
	loader_load_file(FILE_FROM_SYMBOL_FUNC_CALL(kernel), "build/modules/kernel.ko");
	loader_load_file(FILE_FROM_SYMBOL_FUNC_CALL(testing_test), "build/modules/testing/test.ko");
	loader_print_tls_layout(tls_schema);
    struct tls_data *tcb = loader_create_tcb();
	// once everything is patched, we should be able to run test_function
	// which should call our callback!
	void (*test_function)(int, int*) = loader_search_symbol("test_function_02");
	int *(*kernel_function)(void (*)(int, int*), int *) = loader_search_symbol("entrypoint");
	int *module_data = loader_tls_ptr(tcb, (ssize_t)loader_search_symbol("module_data"));
	
    loader_switch_tcb(tcb);
	int *out = kernel_function(test_function, module_data);
	printf("out = { %d, %d, %d, %d }\n", out[0], out[1], out[2], out[3]);
	printf("Done!\n");
	return 0;
}
