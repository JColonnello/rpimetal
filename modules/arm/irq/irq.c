#include "utils.h"
#include <arm/irq.h>
#include <arm/sysregs.h>
#include <attrib.h>
#include <drivers/irq.h>
#include <drivers/mbox.h>
#include <drivers/uart.h>
#include <sglib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct
{
	bool enabled;
	uint32_t mask;
	irq_type type;
	void (*handler)(void *);
	void *param;
} irq_handler;

#define MAX_HANDLERS 32
extern const void vectors;
static irq_handler core_interrupts[MAX_HANDLERS];
static uint32_t *core_interrupt_source;
// Order handlers by enabled, type, and mask
#define HANDLER_ORD(a, b) \
	((a.enabled != b.enabled) \
		 ? ((int)b.enabled - (int)a.enabled) \
		 : ((a.type != b.type) ? ((int)a.type - (int)b.type) : ((int64_t)a.mask - (int64_t)b.mask)))

constructor static void irq_vector_init()
{
	// Set the vector base address to the start of the IRQ vector table
	asm("msr vbar_el1, %0" : : "r"(&vectors));
	// Enable IRQs in the CPU
	irq_enable();
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

void show_invalid_entry_message(int type, unsigned long esr, unsigned long pc, unsigned long far)
{
	printf("%s, ESR: %lx, PC: %lx, FAR: %lx\n", entry_error_messages[type], esr, pc, far);
}

void irq_fiq_handler(void (*handler)(void))
{
	void *place = (void *)&vectors + 0x300;
	memcpy(place, handler, 128);
}

void irq_handle(void)
{
	int i = 0;
	uint32_t handled, source;

	// Handle ARM core and local peripheral interrupts
	for (handled = (1 << 8), source = mreg32(core_interrupt_source);
		 i < MAX_HANDLERS && core_interrupts[i].enabled && core_interrupts[i].type == LOCAL_INTERRUPT;
		 i++)
	{
		if (core_interrupts[i].mask & source)
		{
			handled |= core_interrupts[i].mask;
			if (core_interrupts[i].handler)
				core_interrupts[i].handler(core_interrupts[i].param);
		}
	}
	if (source & ~handled)
		printf("Unhandled core interrupts: 0x%04x\n", source & ~handled);
	if (!(source & (1 << 8)))
		return; // No GPU or basic interrupts pending

	// Handle basic and aliased GPU interrupts
	for (handled = (1 << 8) | (1 << 9), source = mreg32(IRQ_BASIC_PENDING);
		 i < MAX_HANDLERS && core_interrupts[i].enabled && core_interrupts[i].type == BASIC_INTERRUPT;
		 i++)
	{
		if (core_interrupts[i].mask & source)
		{
			handled |= core_interrupts[i].mask;
			if (core_interrupts[i].handler)
				core_interrupts[i].handler(core_interrupts[i].param);
		}
	}
	if (source & ~handled)
		printf("Unhandled basic interrupts: 0x%04x\n", source & ~handled);
	bool pending_gpu1 = source & (1 << 8), pending_gpu2 = source & (1 << 9);

	if (pending_gpu1)
		for (handled = 0, source = mreg32(IRQ_PENDING_1);
			 i < MAX_HANDLERS && core_interrupts[i].enabled && core_interrupts[i].type == GPU_INTERRUPT1;
			 i++)
		{
			if (core_interrupts[i].mask & source)
			{
				handled |= core_interrupts[i].mask;
				if (core_interrupts[i].handler)
					core_interrupts[i].handler(core_interrupts[i].param);
			}
		}
	if (source & ~handled)
		printf("Unhandled GPU interrupts 0-31: 0x%04x\n", source & ~handled);

	if (pending_gpu2)
		for (handled = 0, source = mreg32(IRQ_PENDING_2); i < MAX_HANDLERS && core_interrupts[i].enabled; i++)
		{
			if (core_interrupts[i].mask & source)
			{
				handled |= core_interrupts[i].mask;
				if (core_interrupts[i].handler)
					core_interrupts[i].handler(core_interrupts[i].param);
			}
		}
	if (source & ~handled)
		printf("Unhandled GPU interrupts 32-63: 0x%04x\n", source & ~handled);
}

// Get bit on basic pending register or return -1 if it isn't aliased
static int gpu_to_basic_irq(unsigned int irq)
{
	switch (irq)
	{
	case 7:
		return 10;
	case 9:
		return 11;
	case 10:
		return 12;
	case 18:
		return 13;
	case 19:
		return 14;
	case 53:
		return 15;
	case 54:
		return 16;
	case 55:
		return 17;
	case 56:
		return 18;
	case 57:
		return 19;
	case 62:
		return 20;
	default:
		return -1;
	}
}

static bool check_nIrq(unsigned char *nIrq, irq_type *type)
{
	switch (*type)
	{
	case LOCAL_INTERRUPT:
		if (*nIrq > 17)
			return false; // Local interrupts only support 0-17
		break;
	case BASIC_INTERRUPT:
		if (*nIrq > 7)
			return false; // Basic interrupts only support 0-7
		break;
	case GPU_INTERRUPT1:
		if (*nIrq > 31)
			return false; // GPU interrupts 1 only support 0-31
		goto alias;
	case GPU_INTERRUPT2:
		if (*nIrq < 32 || *nIrq > 63)
			return false; // GPU interrupts only support 32-63

	alias:;
		int alias = gpu_to_basic_irq(*nIrq);
		if (alias >= 0)
		{
			*nIrq = alias;
			*type = BASIC_INTERRUPT;
		}
		else if (*nIrq >= 32)
			*nIrq -= 32; // Convert to 0-31 range
	}
	return true;
}

bool irq_register(void (*handler)(void *), void *param, irq_type type, unsigned char nIrq)
{
	if (core_interrupt_source == NULL)
	{
		core_interrupt_source = (uint32_t *)CORE0_INT_SOURCE + get_core_id();
	}

	if (core_interrupts[MAX_HANDLERS - 1].enabled)
		return false;

	irq_type alias = type;
	unsigned char nAlias = nIrq;
	if (!check_nIrq(&nAlias, &alias))
		return false;

	core_interrupts[MAX_HANDLERS - 1] = (irq_handler){
		.enabled = true,
		.mask = 1 << nAlias,
		.type = alias,
		.handler = handler,
		.param = param,
	};

	irq_disable();
	SGLIB_ARRAY_SINGLE_HEAP_SORT(irq_handler, core_interrupts, MAX_HANDLERS, HANDLER_ORD);
	irq_enable();

	switch (type)
	{
	case BASIC_INTERRUPT:
		mreg32(ENABLE_BASIC_IRQS) = 1 << nIrq;
		break;
	case GPU_INTERRUPT1:
		mreg32(ENABLE_IRQS_1) = 1 << nIrq;
		break;
	case GPU_INTERRUPT2:
		mreg32(ENABLE_IRQS_2) = 1 << (nIrq - 32);
		break;
	case LOCAL_INTERRUPT:
		// Local interrupts are enabled by device
		break;
	}
	return true;
}

bool irq_unregister(irq_type type, unsigned char nIrq)
{
	if (core_interrupt_source == NULL)
	{
		core_interrupt_source = (uint32_t *)CORE0_INT_SOURCE + get_core_id();
	}

	if (!check_nIrq(&nIrq, &type))
		return false;

	irq_handler handler = {
		.enabled = true,
		.mask = 1 << nIrq,
		.type = type,
	};
	bool found;
	int i;
	SGLIB_ARRAY_BINARY_SEARCH(irq_handler, core_interrupts, 0, MAX_HANDLERS, handler, HANDLER_ORD, found, i);
	if (!found)
		return false;

	core_interrupts[i].enabled = false;
	SGLIB_ARRAY_SINGLE_HEAP_SORT(irq_handler, core_interrupts, MAX_HANDLERS, HANDLER_ORD);

	switch (type)
	{
	case BASIC_INTERRUPT:
		mreg32(DISABLE_BASIC_IRQS) = 1 << nIrq;
		break;
	case GPU_INTERRUPT1:
		mreg32(DISABLE_IRQS_1) = 1 << nIrq;
		break;
	case GPU_INTERRUPT2:
		mreg32(DISABLE_IRQS_2) = 1 << (nIrq - 32);
		break;
	case LOCAL_INTERRUPT:
		// Local interrupts are disabled by device
		break;
	}
	return true;
}