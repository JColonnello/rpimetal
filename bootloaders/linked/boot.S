#include "arm/sysregs.h"

.section ".text.boot"

.globl boot
boot:
	mrs	x0, mpidr_el1		
	and	x0, x0,#0xFF		// Check processor id
	cbz	x0, master			// Hang for all non-primary CPU
	b	proc_hang

.globl proc_hang, exit
.weak exit
exit:
	bl libc_fini_array
proc_hang: 
	wfi
	b 	proc_hang

master:
	// Trap other cores
	adr x0, proc_hang
	mov x1, #0xe0
	str x0, [x1], #8
	str x0, [x1], #8
	str x0, [x1]
	sev

	// Enable SIMD and float extensions
	ldr x0, =CPACR_VALUE
	msr cpacr_el1, x0
	ldr	x0, =HCR_VALUE
	msr	hcr_el2, x0
	ldr	x0, =SCTLR_VALUE_MMU_DISABLED
	msr	sctlr_el1, x0		

	/* qemu starts at EL2, than is not possible to access any register of EL3 */
	//ldr	x0, =SCR_VALUE
	//msr	scr_el3, x0
	
jump_el1:
	ldr	x0, =SPSR_VALUE
	msr	spsr_el2, x0		// 'spsr_el3' changed to 'spsr_el2'
	adr	x0, el1_entry		
	msr	elr_el2, x0			// 'elr_el3' changed to 'elr_el2'

	eret

el1_entry:
	adr	x0, __stack
	mov sp, x0
	bl _cpu_init_hook
	b	_boot_setup

.global memset
.weak memset
memset:
	stp x29, x30, [sp, #-16]!
	mov x29, sp
	cmp x2, #0
	beq memset_done
1:
	strb w1, [x0], #1
	subs x2, x2, #1
	bne 1b
memset_done:
	ldp x29, x30, [sp], #16
	ret

.global atexit
.weak atexit
atexit:
	ret

.globl _cpu_init_hook
_cpu_init_hook:
	stp x29, x30, [sp, #-16]!
	mov x29, sp
	// Setup plain memory map
	bl mmu_init
	// Set bss to zero
	ldr x0, .LC3
	mov w1, #0
	ldr x2, .LC4
	sub x2, x2, x0
	bl memset
	// Set tbss to zero
	ldr x0, .LC0
	mov w1, #0
	ldr x2, .LC1
	sub x2, x2, x0
	bl memset
	ldr x0, .LC2
	msr tpidr_el1, x0

	ldp x29, x30, [sp], #16
	ret


.LC0:
	.dword __tbss_start__
.LC1:
	.dword __tbss_end__
.LC2:
	.quad _tcb
.LC3:
	.dword __bss_start__
.LC4:
	.dword __bss_end__