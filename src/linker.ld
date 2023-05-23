ENTRY(boot)

SECTIONS
{
    . = 0x80000;
	__stack = .;
	.text.boot : { *(.text.boot) }
	.text : { *(.text) }
	.rodata : { *(.rodata) }
	.data : { *(.data) }
	. = ALIGN(0x8);
	__bss_start__ = .;
	.bss : { *(.bss*) } 
	__bss_end__ = .;
}
