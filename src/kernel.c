#include <stdio.h>
#include "irq.h"
#include "peripherals/uart.h"
#include "entry.h"
#include "utils.h"
#include "peripherals/irq.h"
#include "peripherals/timer.h"

extern int load_elf();

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
	load_elf();

	printf("Done!\n");
	return 0;
}
