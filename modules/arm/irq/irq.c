#include "utils.h"
#include <arm/irq.h>
#include <attrib.h>
#include <drivers/irq.h>
#include <drivers/mbox.h>
#include <drivers/uart.h>
#include <stdio.h>
#include <string.h>

extern const void vectors;
void handle_timer_irq(void);

constructor void irq_vector_init()
{
	// Set the vector base address to the start of the IRQ vector table
	asm("msr vbar_el1, %0" : : "r"(&vectors));
	// Enable IRQs in the CPU
	enable_irq();
}

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

constructor void enable_interrupt_controller()
{
	// Enable IRQ Core 0 - Pag. 13 BCM2836_ARM-local_peripherals
	// put32(CORE0_INT_CTR, (1 << 1));
	// Enable UART interrupt
	mreg32(ENABLE_IRQS_2) |= 1 << 25;
	// Enable mailbox interrupt
	mreg32(ENABLE_BASIC_IRQS) |= 1 << 1;
	mbox_irq_init();
	// printf("IRQS: %x\n", *(uint32_t*)ENABLE_IRQS_2);

	enable_irq();
}

void show_invalid_entry_message(int type, unsigned long esr, unsigned long address)
{
	printf("%s, ESR: %lx, address: %lx\n", entry_error_messages[type], esr, address);
}

void handle_gpu_irq(unsigned irq)
{
	// printf("IRQ: %x\n", irq);
	for (unsigned handled = 1; handled; irq &= ~handled)
	{
		handled = 0;
		if ((handled = irq & 1 << 19))
			handle_uart0();
		else if ((handled = irq & 1 << 1))
			mbox_read();
		else if ((handled = irq & 1 << 9))
			continue;
		// printf("GPU 2: %x\n", get32(IRQ_PENDING_2));
	}
	if (irq)
		printf("Unknown pending GPU irq: %x\n", irq);
}

void handle_irq(void)
{
	unsigned int irq = mreg32(CORE0_INT_SOURCE);

#define test_bit(bit) (tmp = irq & (1 << (bit)), handled |= tmp, tmp)
	for (unsigned handled = 1, tmp; handled; irq &= ~handled)
	{
		handled = 0;
		if (test_bit(11))
		{
			// handle_timer_irq();
		}
		// Generic timer CNTPNS
		if (test_bit(1))
		{
			// Disable the timer
			asm("msr CNTP_CTL_EL0, %0" : : "r"(0));
		}
		if (test_bit(8))
			handle_gpu_irq(mreg32(IRQ_BASIC_PENDING));
	}
	if (irq)
		printf("Unknown pending irq: %x\n", irq);
}

void register_fiq(void (*handler)(void))
{
	void *place = (void *)&vectors + 0x300;
	memcpy(place, handler, 128);
}
