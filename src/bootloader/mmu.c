#include <stdbool.h>

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

__attribute__ ((section (".bss.mmu")))
__attribute__ ((aligned (0x1000)))
MMU_Page
mmuTable1n[0x200];
__attribute__ ((section (".bss.mmu")))
__attribute__ ((aligned (0x1000)))
MMU_Block
mmuTable1a[0x200],
mmuTable1b[0x200];
__attribute__ ((section (".bss.mmu")))
__attribute__ ((aligned (64)))
MMU_Table
mmuTable0[2];

void *plainMap()
{
    // Setup first level 2 table for 0000 0000h - 3FFF FFFFh
    // First, normal memory: 0000 0000h - 3DFF FFFFh
    for(int i = 0; i < 0x200; i++)
    {
        mmuTable1a[i] = (MMU_Block)
        {
            .oa = ((2 << 20) * i) >> 17,
            .sh = 0b11,
            .uxn = true,
            .ap = 0b00,
            .attrIndx = 0,
            .af = true,
            .contiguous = i >= 16,
            .valid = true,
        };
    }
    // Then, device memory: 3E00 0000h - 3FFF FFFFh
    for(int i = 0x1f0; i < 0x200; i++)
    {
        mmuTable1a[i].attrIndx = 1;
        mmuTable1a[i].pxn = true;
    }
    // Point first 2MB block to a special table to block null references
    ((MMU_Table*)mmuTable1a)[0] = (MMU_Table)
    {
        .nextLevel = ((unsigned long)&mmuTable1n) >> 12,
        .format = 1,
        .valid = 1,
    };

    // Setup second level 2 table for 4000 0000h - 4003 FFFFh
    // Device memory
    for(int i = 0; i < 64; i++)
    {
        mmuTable1b[i] = (MMU_Block)
        {
            .oa = (0x40000000 + (2 << 20) * i) >> 17,
            .sh = 0b11,
            .uxn = true,
            .pxn = true,
            .ap = 0b00,
            .attrIndx = 1,
            .af = true,
            .contiguous = true,
            .valid = true,
        };
    }
    // Set all other entries to 0
    for(int i = 64; i < 0x200; i++)
        mmuTable1b[i] = (MMU_Block){0};

    // Special table for 0h - 20 0000h
    for(int i = 0; i < 0x200; i++)
    {
        mmuTable1n[i] = (MMU_Page)
        {
            .oa = ((1 << 12) * i) >> 12,
            .sh = 0b11,
            .uxn = true,
            .ap = 0b00,
            .attrIndx = 0,
            .af = true,
            .contiguous = true,
            .valid = true,
            .res1 = 1,
        };
    }
    // First 16 entries = 64kb are invalidated
    for(int i = 0; i < 16; i++)
        mmuTable1n[i].valid = false;

    // Configure level 1 table pointing to the previous tables
    mmuTable0[0] = (MMU_Table)
    {
        .nextLevel = ((unsigned long)&mmuTable1a) >> 12,
        .format = 1,
        .valid = 1,
    };
    mmuTable0[1] = (MMU_Table)
    {
        .nextLevel = ((unsigned long)&mmuTable1b) >> 12,
        .format = 1,
        .valid = 1,
    };

    return mmuTable0;
}