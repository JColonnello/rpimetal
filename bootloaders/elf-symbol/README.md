# Bootloader: elf-symbol

A dynamic bootloader that loads the kernel and modules as ELF files at runtime, resolving symbols with the `complex-loader` module (based on libbfd).

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

Similar to `linked/boot.S`, but `el1_entry` branches to `_start` (the C runtime startup linked in by the toolchain) instead of `_boot_setup`, so the bootloader runs as a regular C program with `main()`. `_cpu_init_hook` sets up the MMU and the bootloader's own TLS.

### boot.c

The main bootloader logic that uses the `complex-loader` module:

```c
int main(void)
{
    struct start_symbol symbols[] = {
        {.name = "__stack", .address = (void *)0x80000},
        {.name = "__bss_start__", .address = NULL},
        {.name = "__bss_end__", .address = NULL},
        {.name = "_fini", .address = fini},
    };

    linkset = loader_create_linkset();
    void *mem_end = sbrk(0);
    loader_add_starting_symbols(linkset, sizeof(symbols) / sizeof(*symbols), symbols);

    for (size_t i = 0; i < kernel_payload_count; ++i)
    {
        const struct payload_entry *e = &kernel_payload[i];
        FILE *f = fmemopen((void *)e->ptr, e->size, "rb");
        loader_read_file(linkset, f, e->path);
    }
    if (loader_finish_link(linkset) != LOADER_ERROR_NONE)
        return -1;

    // ... load extra_payload[] one by one, create TCB, setup boot_info
    kernel_start = loader_search_symbol(linkset, "_start");
    return 0;
}
```

Key operations:
1. **Create a linkset**: `loader_create_linkset()` holds the state of one link (files, symbols, TLS layout)
2. **Register base symbols**: Stack, BSS bounds, fini handler
3. **Load kernel files**: Every entry in `kernel_payload[]` is opened with `fmemopen()` and read with `loader_read_file()`, then linked together with a single `loader_finish_link()`
4. **Load extra modules**: Every entry in `extra_payload[]` is read and linked on its own, calling `loader_finish_link()` after each one
5. **Create TLS**: `loader_create_tcb(linkset)` for thread-local storage
6. **Resolve entry point**: `loader_search_symbol(linkset, "_start")`
7. **Jump to kernel**: `main()` returns 0, the C runtime calls `_exit(0)`, and `_exit` (in `stdlib.c`) calls `kernel_jump()`, which switches TCB and branches to the kernel. A non-zero status hangs the CPU instead

### stdlib.c

System call stubs for the C library (`_sbrk`, `_write` over the mux channels 0 and 1, `_fstat`, `_isatty`, `_close`, `sysconf`) and the `_exit` hook described above.

### linker.ld

Identical to `linked/linker.ld`. Payload embedding is not done by the linker script but by `generate_payload.sh` (see below).

### Makefile

Builds the bootloader with embedded kernel/modules:

```makefile
BOOT_MODULES := arm/mmu-basic libc/libc arm/irq drivers/timer sys/mux drivers/mbox drivers/uart complex-loader

$(BUILD_DIR)/kernel8.elf: $(DIR)/linker.ld $(OUTPUT_KO) \
		$(BUILD_DIR)/payloads/kernel_payload.o $(BUILD_DIR)/payloads/extra_payload.o \
		$(BOOT_MODULES:%=$(BUILD_DIR)/$(MODULES_DIR)/%.ko)
	$(CC) $(CFLAGS) -Wl,--unresolved-symbols=ignore-all -T $^ -o $@

$(BUILD_DIR)/payloads/kernel_payload.o: \
	$(BUILD_DIR)/extra/elf-init.ko \
	$(KERNEL_MODULES:%=$(BUILD_DIR)/$(MODULES_DIR)/%.ko) \
	$(BUILD_DIR)/$(KERNEL).ko

$(BUILD_DIR)/payloads/extra_payload.o: \
	$(EXTRA_MODULES:%=$(BUILD_DIR)/$(MODULES_DIR)/%.ko)

$(BUILD_DIR)/payloads/%.o $(BUILD_DIR)/payloads/%.c $(BUILD_DIR)/payloads/%.S &:
	sh bootloaders/elf-symbol/generate_payload.sh $(BUILD_DIR)/payloads "$*" $^
	$(CC) $(CFLAGS) $(INC_FLAGS) -r "$(BUILD_DIR)/payloads/$*.S" "$(BUILD_DIR)/payloads/$*.c" -o $@
```

There are two payloads:
- `kernel_payload.o`: `extra/elf-init.ko`, the `KERNEL_MODULES` and `$(KERNEL).ko`. Linked together as the kernel
- `extra_payload.o`: the `EXTRA_MODULES`. Each one is linked separately on top of the kernel

### generate_payload.sh

Called as `generate_payload.sh <dir> <prefix> <files...>`. It writes two files:
- `<prefix>.S`: embeds each file in `.rodata` with `.incbin`, between the global symbols `<prefix>_f<N>_start` and `<prefix>_f<N>_end` (N starts at 1)
- `<prefix>.c`: a `struct payload_entry <prefix>[]` table with the file path and start pointer of each file, `<prefix>_count`, and a constructor that fills in each entry's `size`

## ELF Loading Process

The API is declared in `include/complex-loader.h`. All functions take the `struct linkset` the files are being linked into.

### 1. Linkset creation

```c
struct linkset *linkset = loader_create_linkset();
```

### 2. Base Symbol Registration

The bootloader provides essential symbols that loaded modules may need:

```c
loader_add_starting_symbols(linkset, n, symbols);  // const struct start_symbol symbols[n]
```

### 3. Reading Files

Each `.ko` file is read via:

```c
enum loader_error err = loader_read_file(linkset, file, "module_name.ko");
```

`file` is a `FILE *`; the filename is used to identify the module (e.g. for GDB).

### 4. Linking

```c
enum loader_error err = loader_finish_link(linkset);
```

Resolves symbols and applies relocations for the files read since the last call. Returns `LOADER_ERROR_NONE` on success, or `LOADER_ERROR_INVALID_FILE`, `LOADER_ERROR_OUT_OF_MEMORY`, `LOADER_ERROR_RELOCATION_FAILED`.

### 5. Symbol Resolution

Symbols from linked modules can be looked up by name:

```c
void *fn = loader_search_symbol(linkset, "function_name");
```

For TLS variables the returned value is an offset into the TCB; `boot.c` converts it to an address with `loader_tls_ptr(tcb, (ssize_t)offset)`.

### 6. TLS Setup

For thread-local storage:

```c
struct tls_data *tcb = loader_create_tcb(linkset);
loader_switch_tcb(tcb);
```

## Embedded Module Access

Each payload object exposes a table of `struct payload_entry` (from `include/payload.h`) and its length:

```c
struct payload_entry
{
    const char *path;
    size_t size;
    const void *ptr;
};

extern const struct payload_entry kernel_payload[];
extern const size_t kernel_payload_count;
extern const struct payload_entry extra_payload[];
extern const size_t extra_payload_count;
```

To get a `FILE *` for the loader, open the embedded data with `fmemopen()`:

```c
const struct payload_entry *e = &kernel_payload[i];
FILE *f = fmemopen((void *)e->ptr, e->size, "rb");
loader_read_file(linkset, f, e->path);
```

## Memory Layout

```
0x00080000  ← __stack
            ← .text.boot (bootloader code)
            ← bootloader .text, .rodata (includes embedded payload .ko files), .data
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
struct boot_customdata
{
    void (*test_function)(int, int *);  // Example: pointer to loaded function (test_function_02)
    int *module_data;                   // Example: pointer to module TLS data (module_data)
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

- `complex-loader`: The ELF loader (links against libbfd)
- `arm/mmu-basic`: MMU initialization
- `arm/irq`: Interrupt handling
- `drivers/uart`: Serial output
- `drivers/timer`: Timing functions
- `sys/mux`: Serial multiplexing
- `drivers/mbox`: Mailbox communication
- `libc/libc`: Standard C library

## Debugging

Symbol information for dynamically loaded modules can be tricky. The loader can print the TLS layout (`boot.c` already does this before creating the TCB):

```c
loader_print_tls_layout(linkset);
```

For GDB, loaded module addresses are added automatically to the memory layout by the `gdb-commands.py` plugin.

## Limitations

- **Larger binary**: Bootloader includes libbfd and loader code
- **Slower boot**: ELF parsing and relocation takes time
- **Memory overhead**: Each loaded module requires extra space for its symbol tables and other metadata
- **Complexity**: More moving parts than static linking

For simpler use cases, consider [linked](../linked/README.md) instead.
