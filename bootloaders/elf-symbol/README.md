# Bootloader: elf-symbol

A dynamic bootloader that loads the kernel and modules as ELF files at runtime, resolving symbols using libbfd.

## Overview

Unlike the `linked` bootloader which statically links everything at build time, `elf-symbol` includes an ELF loader that can:

- Load `.ko` files (relocatable ELF objects) at runtime
- Resolve symbols dynamically between modules
- Support thread-local storage (TLS)
- Allow modules to be loaded from embedded binary data or external sources

## When to Use

- **Complex programs**: When you need the full module system
- **Dynamic loading**: When you want to load modules at runtime
- **Thread-local storage**: When using `__thread` variables
- **Development**: When you want to hot-swap modules (with extensions)
- **Educational**: When studying ELF loading and symbol resolution

## Files

### boot.S

Similar to `linked/boot.S` but with additional features:

1. **Standard boot sequence**: Core detection, EL2→EL1 transition, stack setup
2. **Built-in `memset`**: For BSS/TLS initialization
3. **`_cpu_init_hook`**: Initializes MMU and TLS before C code runs

```asm
.globl _cpu_init_hook
_cpu_init_hook:
    // Setup plain memory map
    bl mmu_init
    // Set tbss to zero
    ldr x0, .LC0
    mov w1, #0
    ldr x2, .LC1
    sub x2, x2, x0
    bl memset
    // Set thread pointer
    ldr x0, .LC2
    msr tpidr_el1, x0
    ret
```

### boot.c

The main bootloader logic that uses the `loader` module:

```c
int main(void)
{
    symbol_data symbols[] = {
        {.name = "__stack", .address = (void *)0x80000},
        {.name = "__bss_start__", .address = NULL},
        {.name = "__bss_end__", .address = NULL},
        {.name = "_fini", .address = fini},
    };

    void *mem_end = sbrk(0);
    loader_init();
    loader_add_starting_symbols(sizeof(symbols) / sizeof(*symbols), symbols);
    loader_load_file(FILE_FROM_SYMBOL_FUNC_CALL(kernel), "build/kernel.ko");
    
    // ... setup boot_info and jump to kernel
}
```

Key operations:
1. **Initialize loader**: `loader_init()` sets up libbfd
2. **Register base symbols**: Stack, BSS bounds, fini handler
3. **Load kernel ELF**: Parse and load the kernel from embedded binary
4. **Create TLS**: `loader_create_tcb()` for thread-local storage
5. **Resolve entry point**: `loader_search_symbol("_start")`
6. **Jump to kernel**: `kernel_jump()` switches TCB and branches

### linker.ld

Extended linker script that also handles payload embedding:

- Same structure as `linked/linker.ld`
- Payload `.ko` files are embedded as binary data
- Symbols like `_binary_build_kernel_ko_start` reference embedded files

### Makefile

Builds the bootloader with embedded kernel/modules:

```makefile
BOOT_MODULES := arm/mmu-basic libc/libc arm/irq drivers/timer sys/mux \
                drivers/mbox drivers/uart loader

$(BUILD_DIR)/kernel8.elf: $(DIR)/linker.ld $(OUTPUT_KO) $(BUILD_DIR)/payload.o $(BOOT_MODULES)
    $(CC) $(CFLAGS) -Wl,--unresolved-symbols=ignore-all -T $^ -o $@

$(BUILD_DIR)/payload.o: $(EXTRA_MODULES) $(BUILD_DIR)/kernel.ko
    $(LD) -r -b binary $^ -o $@
```

The `payload.o` rule uses `ld -b binary` to embed `.ko` files as raw binary data, accessible via symbols like:
- `_binary_build_kernel_ko_start`
- `_binary_build_kernel_ko_end`

## ELF Loading Process

### 1. Initialization

```c
loader_init();  // Initialize libbfd
```

### 2. Base Symbol Registration

The bootloader provides essential symbols that loaded modules may need:

```c
loader_add_starting_symbols(n, symbols);
```

### 3. Loading Modules

Each `.ko` file is loaded via:

```c
loader_load_file(file_handle, "module_name.ko");
```

This:
- Parses ELF headers using libbfd
- Allocates memory for sections
- Copies section data
- Performs relocations
- Registers exported symbols

### 4. Symbol Resolution

Modules can find symbols from other loaded modules:

```c
void *fn = loader_search_symbol("function_name");
```

### 5. TLS Setup

For thread-local storage:

```c
struct tls_data *tcb = loader_create_tcb();
loader_switch_tcb(tcb);
```

## Embedded Module Access

Modules are embedded using the `FILE_FROM_SYMBOL` macros:

```c
// Declare access to embedded module
FILE_FROM_SYMBOL_FUNC_DECL(kernel);

// Get a FILE* to read the embedded data
FILE *f = FILE_FROM_SYMBOL_FUNC_CALL(kernel);
loader_load_file(f, "kernel.ko");
```

The macro expands to use `fmemopen()` on the embedded binary data.

## Memory Layout

```
0x00080000  ← __stack
            ← .text.boot (bootloader code)
            ← bootloader .text, .rodata, .data
            ← embedded payload (.ko files)
            ← bootloader .bss
            ← bootloader heap (sbrk)
            ← loaded module sections (dynamically allocated)
            ...
0x3E000000  ← memory_end
```

## Boot Info

The kernel receives extended boot information:

```c
boot_info = (struct boot_info){
    .boot_memory_end = mem_end,      // After bootloader's heap
    .memory_start = sbrk(0),         // Current heap position
    .memory_end = (void *)0x3E000000,
};
```

Plus optional custom data via `boot_userdata.custom`:

```c
struct boot_customdata {
    void *test_function;  // Example: pointer to loaded function
    void *module_data;    // Example: pointer to module TLS data
};
```

## Example Usage

In `config.mk`:

```makefile
BOOTLOADER = bootloaders/elf-symbol
KERNEL = examples/0A_misc
EXTRA_MODULES = testing/test
```

Your kernel can use all features:

```c
#include <boot.h>
#include <boot/custom.h>

int kernel_start(struct boot_info *info, union boot_userdata userdata) {
    struct boot_customdata *custom = userdata.custom;
    
    // Use dynamically loaded modules
    printf("Memory available: %p - %p\n", 
           info->memory_start, info->memory_end);
    
    return 0;
}
```

## Dependencies

This bootloader requires several modules to be available:

- `loader`: The ELF loader using libbfd
- `arm/mmu-basic`: MMU initialization
- `arm/irq`: Interrupt handling
- `drivers/uart`: Serial output
- `drivers/timer`: Timing functions
- `sys/mux`: Serial multiplexing
- `drivers/mbox`: Mailbox communication
- `libc/libc`: Standard C library

## Debugging

Symbol information for dynamically loaded modules can be tricky. The loader can print TLS layout:

```c
loader_print_tls_layout(tls_schema);
```

For GDB, loaded module addresses need to be added manually using `add-symbol-file`.

## Limitations

- **Larger binary**: Bootloader includes libbfd and loader code
- **Slower boot**: ELF parsing and relocation takes time
- **Memory overhead**: Each loaded module allocates sections dynamically
- **Complexity**: More moving parts than static linking

For simpler use cases, consider [linked](../linked/) instead.
