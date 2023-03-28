#include "utils.h"
#include "printf.h"
#include "entry.h"
#include "peripherals/irq.h"
#include "peripherals/timer.h"
#include "peripherals/uart.h"
#include "mbox.h"
#include <stdint.h>

const char *entry_error_messages[] = {
	"SYNC_INVALID_EL1t",
	"IRQ_INVALID_EL1t",		
	"FIQ_INVALID_EL1t",		
	"ERROR_INVALID_EL1T",		

	"SYNC_INVALID_EL1h",		
	"IRQ_INVALID_EL1h",		
	"FIQ_INVALID_EL1h",		
	"ERROR_INVALID_EL1h",		

	"SYNC_INVALID_EL0_64",		
	"IRQ_INVALID_EL0_64",		
	"FIQ_INVALID_EL0_64",		
	"ERROR_INVALID_EL0_64",	

	"SYNC_INVALID_EL0_32",		
	"IRQ_INVALID_EL0_32",		
	"FIQ_INVALID_EL0_32",		
	"ERROR_INVALID_EL0_32"	
};

void enable_interrupt_controller()
{
	// Enable IRQ Core 0 - Pag. 13 BCM2836_ARM-local_peripherals
	// put32(CORE0_INT_CTR, (1 << 1));
	// Enable UART interrupt
	put32(ENABLE_IRQS_2, 1<<25);
	put32(ENABLE_BASIC_IRQS, (1<<1 | 1<<2 | 1<<3));
	put32(CORE0_INT_SOURCE, 1<<8);
	mbox_irq();
	// printf("IRQS: %x\n", *(uint32_t*)ENABLE_IRQS_2);
}

void show_invalid_entry_message(int type, unsigned long esr, unsigned long address)
{
	printf("%s, ESR: %x, address: %x\r\n", entry_error_messages[type], esr, address);
}

static void handle_gpu_irq()
{
	switch (get32(IRQ_BASIC_PENDING))
	{
		case 1<<10 ... (1<<21-1):
			handle_uart0;
			break;
		case 1<<1:
			mbox_read();
			break;
		default:
			break;
	}
}

void handle_irq(void)
{
	unsigned int irq = get32(CORE0_INT_SOURCE);
	switch (irq) {
		case (0x800):
			handle_timer_irq();
			break;
		case (0x100):
			handle_gpu_irq();
			break;
		default:
			printf("Unknown pending irq: %x\r\n", irq);
	}
}
