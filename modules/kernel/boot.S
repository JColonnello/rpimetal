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
	
	// Setup desctructors to be called at exit
	mov x19, x0
	adr x0, __libc_fini_array
	bl atexit

	// Call constructors
	bl __libc_init_array

	// Call main and exit with returned status code
	mov x0, x19
	bl kernel_start
	bl exit

.Lstack:
	.dword __stack
	.weak __stack