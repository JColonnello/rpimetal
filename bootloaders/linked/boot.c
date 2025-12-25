#include <attrib.h>
#include <boot.h>
#include <stddef.h>

extern noreturn void _start(struct boot_info *info, union boot_userdata userdata);
extern const void __end;
static struct boot_info info;

void noreturn _boot_setup()
{
	union boot_userdata userdata;
	info = (struct boot_info){
		.boot_memory_end = (void *)&__end,
		.memory_start = (void *)&__end,
		.memory_end = (void *)0x3E000000,
	};
	userdata.ptr = NULL;
	_start(&info, userdata);
}