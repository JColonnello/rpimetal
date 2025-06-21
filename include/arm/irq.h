#pragma once
#include <stdbool.h>

typedef enum
{
	LOCAL_INTERRUPT,
	BASIC_INTERRUPT,
	GPU_INTERRUPT1,
	GPU_INTERRUPT2,
} irq_type;

void irq_enable(void);
void irq_disable(void);
bool irq_register(void (*handler)(void *), void *param, irq_type type, unsigned char nIrq);
bool irq_unregister(irq_type type, unsigned char nIrq);
void irq_fiq_handler(void (*handler)(void));
