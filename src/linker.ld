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
	.tdata : 
	{ 
		__tdata_start__ = .;
		. = . + 0x10;
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
