.text
.globl _start
_start:
	// Align and set stack pointer
	ldr x9, .Lstack
	and	x9, x9, ~0xf
	mov	sp, x9

	/* Setup an initial dummy frame with saved fp=0 and saved lr=0 */
	mov	x29, 0
	stp	x29, x29, [sp, #-16]!
	mov	x29, sp
	stp x0, x1, [sp, #-16]!
	
	// Setup desctructors to be called at exit
	adr x0, libc_fini_array
	bl atexit

	// Call constructors
	bl libc_init_array

	// Call main and exit with returned status code
	ldp x0, x1, [sp], #16
	bl kernel_start
	bl exit

.Lstack:
	.dword __stack
	.weak __stack