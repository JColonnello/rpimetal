#include "boot.h"
#include "complex-loader.h"
#include <arm/irq.h>
#include <attrib.h>
#include <boot/custom.h>
#include <drivers/irq.h>
#include <drivers/timer.h>
#include <drivers/uart.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/unistd.h>

#define CONCAT(p1, p2) p1##p2
#define EVALUATOR(p1, p2) CONCAT(p1, p2)
#define _BINARY_SYMBOL_PREFIX(SYMBOL) CONCAT(_binary_build_, SYMBOL)
#define _BINARY_START(NAME) EVALUATOR(_BINARY_SYMBOL_PREFIX(NAME), _ko_start)
#define _BINARY_END(NAME) EVALUATOR(_BINARY_SYMBOL_PREFIX(NAME), _ko_end)
#define FILE_FROM_SYMBOL_FUNC_CALL(NAME) _##NAME##_get_file()
#define FILE_FROM_SYMBOL_FUNC_DECL(NAME) \
	extern char _BINARY_START(NAME)[], _BINARY_END(NAME)[]; \
	FILE *_##NAME##_get_file() \
	{ \
		return fmemopen(_BINARY_START(NAME), (size_t)(_BINARY_END(NAME) - _BINARY_START(NAME)), "rb"); \
	}

FILE_FROM_SYMBOL_FUNC_DECL(modules_testing_test);
FILE_FROM_SYMBOL_FUNC_DECL(kernel);

void fini()
{
}

static struct boot_customdata boot_data;
static struct boot_info boot_info;
static union boot_userdata boot_userdata;
static start_function_type kernel_start;
static struct tls_data *tcb;

void noreturn kernel_jump()
{
	loader_switch_tcb(tcb);
	//Inline asm equivalent to kernel_start(&boot_info, boot_userdata) without link
	asm("mov x0, %0\n"
		"ldr x1, %1\n"
		"ldr x2, %2\n"
		// Point LR to inside the start function to avoid GDB crash
		"add x30, x2, #4\n"
		"br x2\n"
		:
		: "r"(&boot_info), "m"(boot_userdata), "m"(kernel_start)
		: "x0", "x1", "x2", "x30");
	__builtin_unreachable();
}

int main(void)
{
	struct start_symbol symbols[] = {
		{.name = "__stack", .address = (void *)0x80000},
		{.name = "__bss_start__", .address = NULL},
		{.name = "__bss_end__", .address = NULL},
		{.name = "_fini", .address = fini},
	};

	struct linkset *linkset = loader_create_linkset();
	void *mem_end = sbrk(0);
	loader_add_starting_symbols(linkset, sizeof(symbols) / sizeof(*symbols), symbols);

	FILE *kernel_file = FILE_FROM_SYMBOL_FUNC_CALL(kernel);
	FILE *modules_testing_test_file = FILE_FROM_SYMBOL_FUNC_CALL(modules_testing_test);

	loader_read_file(linkset, kernel_file, "build/kernel.ko");
	loader_read_file(linkset, modules_testing_test_file, "build/modules/testing/test.ko");
	loader_finish_link(linkset);

	loader_print_tls_layout(linkset);
	tcb = loader_create_tcb(linkset);
	// once everything is patched, we should be able to run test_function
	// which should call our callback!

	boot_info = (struct boot_info){
		.boot_memory_end = mem_end,
		.memory_start = sbrk(0),
		.memory_end = (void *)0x3E000000,
	};
	boot_data = (struct boot_customdata){
		.test_function = loader_search_symbol(linkset, "test_function_02"),
		.module_data = loader_tls_ptr(tcb, (ssize_t)loader_search_symbol(linkset, "module_data")),
	};
	boot_userdata.custom = &boot_data;
	kernel_start = loader_search_symbol(linkset, "_start");

	fputs("Jumping to kernel...\n", stdout);

	return 0;
}
