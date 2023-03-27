#include "printf.h"
#include "irq.h"
#include "peripherals/uart.h"
#include "entry.h"
#include "utils.h"
#include "peripherals/irq.h"
#include "peripherals/timer.h"

void kernel_main(void)
{
	uart_init();
	init_printf(0, putc);
	irq_vector_init();
	timer_init();
	enable_interrupt_controller();
	enable_irq();

	// Exception test
    // unsigned int r=*((volatile unsigned int*)0xFFFFFFFFFF000000);
	// r++;

	while (1){
		// uart_send(uart_recv());
	}	
}
