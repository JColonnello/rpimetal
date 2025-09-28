.globl get_el
get_el:
	mrs x0, CurrentEL
	lsr x0, x0, #2
	ret

.globl get_cnt
get_cnt:
	mrs x0, CNTPCT_EL0
	lsr x0, x0, #2
	ret

.globl put32
put32:
	str w1,[x0]
	ret

.globl get32
get32:
	ldr w0,[x0]
	ret

.globl delay
delay:
	subs x0, x0, #1
	bne delay
	ret
	
.macro stub func
.section .text
.align 4
.global \func
\func:
	adr x0, 0f
	b undef_func
	ret
0:
.string "\func"
.endm

stub _kill
stub _getpid
stub getwd
stub lstat
stub posix_memalign
stub _stat
stub _unlink
stub chmod
stub umask
