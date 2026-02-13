#include "boot.h"
#include "loader.h"
#include <arm/irq.h>
#include <attrib.h>
#include <boot/custom.h>
#include <drivers/irq.h>
#include <drivers/timer.h>
#include <drivers/uart.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/unistd.h>

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
	symbol_data symbols[] = {
		{.name = "__stack", .address = (void *)0x80000},
		{.name = "__bss_start__", .address = NULL},
		{.name = "__bss_end__", .address = NULL},
	};

	void *mem_end = sbrk(0);
	loader_init();

	loader_print_tls_layout(tls_schema);
	tcb = loader_create_tcb();
	// once everything is patched, we should be able to run test_function
	// which should call our callback!

	boot_info = (struct boot_info){
		.boot_memory_end = mem_end,
		.memory_start = sbrk(0),
		.memory_end = (void *)0x3E000000,
	};
	boot_data = (struct boot_customdata){
		.test_function = loader_search_symbol("test_function_02"),
		.module_data = loader_tls_ptr(tcb, (ssize_t)loader_search_symbol("module_data")),
	};
	boot_userdata.custom = &boot_data;
	kernel_start = loader_search_symbol("_start");

	fputs("Jumping to kernel...\n", stdout);

	return 0;
}
