#include "arm/sysregs.h"

#include "mm.h"

.section ".text.boot"

.globl boot
boot:
	mrs	x0, mpidr_el1		
	and	x0, x0,#0xFF		// Check processor id
	cbz	x0, master			// Hang for all non-primary CPU
	b	proc_hang

proc_hang: 
	wfi
	b 	proc_hang

master:
	adr x0, proc_hang
	mov x1, #0xe0
	str x0, [x1], #8
	str x0, [x1], #8
	str x0, [x1]

	ldr	x0, =SCTLR_VALUE_MMU_DISABLED
	msr	sctlr_el1, x0		

	ldr	x0, =HCR_VALUE
	msr	hcr_el2, x0

	ldr x0, =CPACR_VALUE
	msr cpacr_el1, x0

	/* qemu starts at EL2, than is not possible to access any register of EL3 */
	//ldr	x0, =SCR_VALUE
	//msr	scr_el3, x0
	
	ldr	x0, =SPSR_VALUE
	msr	spsr_el2, x0		// 'spsr_el3' changed to 'spsr_el2'

	adr	x0, el1_entry		
	msr	elr_el2, x0			// 'elr_el3' changed to 'elr_el2'

	eret				

el1_entry:
	mov	sp, #LOW_MEMORY
	b	_start
	b 	proc_hang		// should never come here

// Stubs
.globl _exit, _close, _lseek, _read, _write, _sbrk
_exit:
_close:
_lseek:
_read:
_write:
_sbrk:

b proc_hang