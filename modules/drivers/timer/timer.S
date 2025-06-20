#include <drivers/base.h>
#include "timer.h"

.globl handle_timer_irq
handle_timer_irq:
	stp x0, x1, [sp, #-16]!
	// Load, increment, and store the counter
	ldr x1, =counter1
	ldr x0, [x1]
	add x0, x0, #1
	str x0, [x1]

	// Store generic timer value
	mrs x0, CNTPCT_EL0
	ldr x1, =counter2
	str x0, [x1]

	// Clear interrupt flag
	ldr x1, =TIMER_FLAG
	mov x0, (1 << 31)
	str x0, [x1]

	ldp x0, x1, [sp], #16
	eret

.global stop_generic_timer
stop_generic_timer:
	msr CNTP_CTL_EL0, xzr
	eret
