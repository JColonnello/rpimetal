# Bootloader: linked

The simplest bootloader that statically links the kernel with the startup code at build time.

## Overview

This bootloader compiles and links your kernel code together with the boot assembly into a single executable. All symbols are resolved at link time, producing a straightforward binary with no runtime loading.

## When to Use

- **Getting started**: Best for learning and simple experiments
- **Self-contained programs**: When you don't need dynamic module loading
- **Minimal overhead**: Smallest boot time and binary size
- **Simple debugging**: All code is in one binary with complete symbols

## Files

### boot.S

Assembly startup code that:

1. **Identifies the primary core**: Uses `mpidr_el1` to check processor ID; secondary cores enter `proc_hang`
2. **Parks secondary cores**: Writes `proc_hang` address to mailbox registers (0xe0, 0xe8, 0xf0)
3. **Enables SIMD/FPU**: Sets `CPACR_VALUE` to enable floating-point and SIMD
4. **Configures system registers**: Sets up `HCR_EL2` and `SCTLR_EL1` for EL1 operation
5. **Drops to EL1**: Uses `eret` to transition from EL2 to EL1
6. **Sets up stack**: Stack pointer set to `__stack` (0x80000, growing downward)
7. **Calls initialization**: Jumps to `_cpu_init_hook` then `_boot_setup`

```asm
el1_entry:
    adr  x0, __stack
    mov  sp, x0
    bl   _cpu_init_hook
    b    _boot_setup
```

### boot.c

Minimal C initialization that prepares `boot_info` and calls the kernel:

```c
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
```

The kernel receives:
- `info->memory_start`: First usable address after the kernel image
- `info->memory_end`: 0x3E000000 (below GPU-reserved memory)

### linker.ld

Linker script that:

1. **Sets entry point**: `ENTRY(boot)`
2. **Places code at 0x80000**: The address where the GPU loads `kernel8.img`
3. **Defines stack location**: `__stack = 0x80000` (stack grows downward)
4. **Orders sections**: .text.boot first, then init arrays, text, rodata, data, bss, TLS
5. **Handles TLS**: Defines `.tdata` and `.tbss` sections with proper alignment

Key symbols defined:
- `__stack`: Stack start address
- `__bss_start__`, `__bss_end__`: BSS section bounds
- `__preinit_array_start/end`, `__init_array_start/end`, `__fini_array_start/end`: Constructor/destructor arrays
- `_tcb`: Thread Control Block for TLS
- `__end`: End of the kernel image

### Makefile

Build rules that:

1. **Define boot modules**: MMU module linked with bootloader
2. **Link the final ELF**: Combines bootloader, kernel, and modules
3. **Resolve unresolved symbols**: Uses `--unresolved-symbols=ignore-all` to allow weak/optional symbols

```makefile
BOOT_MODULES := arm/mmu-basic

$(OUTPUT_DIR).elf: $(DIR)/linker.ld $(OBJ_FILES) $(BUILD_DIR)/kernel.ko $(BOOT_MODULES)
    $(CC) $(CFLAGS) -nostdlib -Wl,--unresolved-symbols=ignore-all -T $^ -o $@
```

## Memory Layout

```
0x00080000  ← __stack (stack grows down)
            ← .text.boot (boot code)
            ← .init_array (constructors)
            ← .text (code)
            ← .rodata (read-only data)
            ← .data (initialized data)
            ← .bss (uninitialized data)
            ← .tdata/.tbss (thread-local storage)
            ← __end (heap starts here)
            ...
0x3E000000  ← memory_end (GPU memory starts)
```

## Example Usage

In `config.mk`:

```makefile
BOOTLOADER = bootloaders/linked
KERNEL = examples/no-libc-uart
```

Your kernel just needs to provide `kernel_start`:

```c
#include <boot.h>

int kernel_start(struct boot_info *info, union boot_userdata userdata) {
    // Your code here
    return 0;
}
```

## Limitations

- **No runtime loading**: All modules must be linked at build time
- **Fixed configuration**: Cannot change modules without rebuilding
- **No dynamic symbols**: Cannot look up symbols by name at runtime

For dynamic loading capabilities, use [elf-symbol](../elf-symbol/README.md) instead.
