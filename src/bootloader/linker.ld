ENTRY(boot)

SECTIONS
{
    . = 0x80000;
	__stack = .;
	.text.boot : { *(.text.boot) }
	.text : { *(.text) }
	.rodata : { *(.rodata) }
	.data : { *(.data) }
	.bss : 
	{
		*(.bss.mmu);
		__bss_start__ = .;
		*(.bss*) 
		__bss_end__ = .;
	}
	. = ALIGN(0x10); /* Align the location counter before _tcb */
	_tcb = .;        /* Assign _tcb to the aligned address */
	. = . + 0x10;   /* Reserve 0x10 bytes after _tcb */
	.tdata : 
	{ 
		*(.tdata) 
	}
	.tbss : 
	{
		__tbss_start__ = .;
		*(.tbss*) 
		__tbss_end__ = .;
	}
	__end = ADDR(.tbss) + (__tbss_end__ - __tbss_start__);
}
