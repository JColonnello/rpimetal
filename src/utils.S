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
.section .rodata
0:
.string "\func"
.section .text
.global \func
\func:
	adr x0, 0b
	b undef_func
	ret
.endm

stub _lseek
stub _read
stub _kill
stub _getpid
stub _gettimeofday
stub getwd
stub lstat
stub _open
stub posix_memalign
stub _stat
stub _unlink
stub chmod
stub umask
