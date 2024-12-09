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
		. = ALIGN(0x1000);
		*(.bss.mmu);
		__bss_start__ = .;
		*(.bss*) 
		__bss_end__ = .;
	} 
}
