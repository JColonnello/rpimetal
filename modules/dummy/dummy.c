#include <arm/mmu-basic.h>
#include <arm/sysregs.h>
#include <attrib.h>
#include <stddef.h>

typedef struct __attribute__((packed))
{
	bool valid : 1;
	unsigned format : 1;
	unsigned : 6;
	unsigned nextLevel2 : 2;
	unsigned : 2;
	unsigned long nextLevel : 38;
	unsigned : 9;
	bool pxn : 1;
	bool uxn : 1;
	unsigned ap : 2;
	bool ns : 1;
} MMU_Table;

typedef struct __attribute__((packed))
{
	bool valid : 1;
	unsigned : 1;
	unsigned attrIndx : 3;
	bool ns : 1;
	unsigned ap : 2;
	unsigned sh : 2;
	bool af : 1;
	bool ng : 1;
	unsigned oa2 : 4;
	bool nT : 1;
	unsigned long oa : 33;
	bool gp : 1;
	bool dbm : 1;
	bool contiguous : 1;
	bool pxn : 1;
	bool uxn : 1;
	int : 4;
	unsigned pbha : 4;
	int : 1;
} MMU_Block;

typedef struct __attribute__((packed))
{
	bool valid : 1;
	unsigned res1 : 1;
	unsigned attrIndx : 3;
	bool ns : 1;
	unsigned ap : 2;
	unsigned sh : 2;
	bool af : 1;
	bool ng : 1;
	unsigned long oa : 38;
	bool gp : 1;
	bool dbm : 1;
	bool contiguous : 1;
	bool pxn : 1;
	bool uxn : 1;
	int : 4;
	unsigned pbha : 4;
	int : 1;
} MMU_Page;

typedef union
{
	MMU_Table table;
	MMU_Block block;
	MMU_Page page;
} MMU_Entry;

static __attribute__((section(".bss.dummy"))) __attribute__((aligned(0x1000))) MMU_Block dummy1b[0x200];
static __attribute__((section(".bss.dummy"))) __attribute__((aligned(64))) MMU_Table dummy0[2];
