# Kernel Modules

RPiMetal uses a modular architecture where functionality is organized into reusable **modules**. Modules are compiled as relocatable ELF objects (`.ko` files) that can be linked statically or loaded dynamically at runtime.

## Overview

### What Are Modules?

Modules are self-contained units of functionality — drivers, libraries, or system services — that:

- Are compiled separately from the kernel/program
- Can be combined in different configurations
- Expose symbols (functions, variables) for use by other modules
- May have dependencies on other modules

### Module Files

Each module produces a `.ko` file (kernel object). Despite the extension, these are **not** Linux kernel modules — they are standard ELF relocatable objects created with:

```bash
$(CC) -r source.c.o -o module.ko
```

The `-r` flag produces relocatable output that can be linked again later.

### Module Loading

Modules can be loaded in two ways:

1. **Static linking** (with `bootloaders/linked`): All modules are linked at build time into a single executable.

2. **Dynamic loading** (with `bootloaders/elf-symbol` or `bootloaders/elf-uart`): Modules are loaded at runtime by the `complex-loader` ELF loader, with symbols resolved dynamically using libbfd.

## Module Categories

### arm/ — ARM Architecture Support

Low-level ARM-specific functionality.

| Module | Description |
|--------|-------------|
| `arm/irq` | Interrupt handling. Sets up the exception vector table, handles IRQ/FIQ, provides `irq_enable()`, `irq_disable()`. |
| `arm/mmu-basic` | Basic MMU setup. Configures page tables, enables virtual memory, provides identity mapping. |

### drivers/ — Hardware Drivers

Drivers for Raspberry Pi peripherals.

| Module | Description |
|--------|-------------|
| `drivers/uart` | Full-featured PL011 UART driver with buffered I/O, callbacks, configurable baud rate. |
| `drivers/simple-uart` | Minimal UART driver for basic output. Lightweight implementation with optional interrupt-driven TX/RX; does not integrate with `sys/mux`. |
| `drivers/timer` | ARM generic timer and BCM2837 local timer support. Provides `timer_monotonic()` for timing. |
| `drivers/mbox` | Mailbox interface for GPU communication. Used to query/set system properties. Also provides `mbox_power_off()` and `mbox_reset()`. |
| `drivers/display` | Framebuffer driver. Sets up screen resolution, provides `lfb_showpicture()` for drawing. |
| `drivers/sd` | SD card driver (EMMC interface). Low-level block read/write. |
| `drivers/sd2` | Alternative SD card implementation. |

### sys/ — System Services

System-level utilities and services.

| Module | Description |
|--------|-------------|
| `sys/mux` | Serial multiplexing. Enables multiple channels over single UART. See [Multiplexing](../docs/multiplexing.md). |
| `sys/printf` | Lightweight printf (Kustaa Nyholm's tfp_printf). Provides `init_printf(putp, putf)` to set the character output function, plus `tfp_printf()`, `tfp_sprintf()` and `tfp_puts()`. `printf` and `sprintf` are weak aliases of `tfp_printf` and `tfp_sprintf`. |

### fs/ — Filesystems

Filesystem implementations.

| Module | Description |
|--------|-------------|
| `fs/fat` | FAT32 filesystem using FatFs library. Requires `drivers/sd2`. |

### complex-loader/, loader/ — ELF Loaders

The dynamic module loaders.

| Module | Description |
|--------|-------------|
| `complex-loader` | ELF loader using libbfd. Loads `.ko` files into a linkset, resolves symbols, collects constructors/destructors, supports TLS. Used by the `elf-symbol` and `elf-uart` bootloaders. Header: `include/complex-loader.h`. |
| `loader` | Older, simpler ELF loader using libbfd with a single global symbol table. Not used by the current bootloaders. Header: `include/loader.h`. |

**Key `complex-loader` functions:**
- `loader_create_linkset()` — Create a linkset (libbfd is initialized by a constructor)
- `loader_add_starting_symbols(linkset, n, symbols)` — Predefine symbols for the linkset
- `loader_read_file(linkset, file, name)` — Read an ELF module from a `FILE*` into the linkset
- `loader_finish_link(linkset)` — Allocate sections and apply relocations for the files read so far
- `loader_search_symbol(linkset, name)` — Find a symbol by name
- `loader_create_tcb(linkset)` — Create a Thread Control Block for TLS

### libc/ — C Library Support

Standard C library components.

| Module | Description |
|--------|-------------|
| `libc/libc` | Newlib C library (pre-compiled). |
| `libc/libgcc` | GCC runtime support library. |

### testing/ — Test Modules

Modules for testing and development.

| Module | Description |
|--------|-------------|
| `testing/test` | Example test module. |
| `testing/test2` | Additional test module. |
| `testing/test3` | Multi-file test module example. |

### dummy/

| Module | Description |
|--------|-------------|
| `dummy` | Placeholder module demonstrating module structure. |

## Module Lists

Modules are registered in `modules/Makefile`:

```makefile
# Standard modules (single-folder, compiled individually)
STD_MODULES = testing/test testing/test2 drivers/uart drivers/mbox loader arm/irq arm/mmu-basic drivers/display drivers/timer \
sys/mux drivers/sd drivers/sd2 dummy sys/printf drivers/simple-uart fs/fat complex-loader

# Multi-file modules (may span multiple source files or have special build rules)
MULTI_MODULES = testing/test3 libc
```

These lists only tell the top-level Makefile which module Makefiles to include (`-include` for `STD_MODULES`, `include` for `MULTI_MODULES`). The `.ko` itself is built by the generic `$(BUILD_DIR)/%.ko` rule for any module named in `KERNEL_MODULES`, `BOOT_MODULES` or `EXTRA_MODULES`, whether or not it is listed here. Multi-modules are the exception: they must define their own rules.

## Creating a New Module

### 1. Create the Module Directory

```
modules/
+-- drivers/
    +-- mymodule/
        +-- mymodule.c
        +-- Makefile    (optional)
```

### 2. Write the Source Code

```c
// modules/drivers/mymodule/mymodule.c
#include <drivers/mymodule.h>
#include <stdio.h>

static int internal_state = 0;

void mymodule_init(void) {
    internal_state = 42;
    puts("mymodule initialized");
}

int mymodule_get_value(void) {
    return internal_state;
}
```

### 3. Create the Header

```c
// include/drivers/mymodule.h
#pragma once

void mymodule_init(void);
int mymodule_get_value(void);
```

### 4. Create the Module Makefile (Optional)

```makefile
# modules/drivers/mymodule/Makefile

# Optional: Additional libraries to link
# $(OUTPUT_KO): LDLIBS += -lsomelib

# Optional: Additional compiler flags
# CFLAGS += -DSOME_DEFINE
```

Most modules need no Makefile at all: the default rules handle compilation.

### 5. Register the Module (Only If It Has a Makefile)

If the module has its own Makefile, add it to `modules/Makefile` so that Makefile gets included:

```makefile
STD_MODULES = ... drivers/mymodule
```

Without a Makefile this step isn't needed: listing the module in `KERNEL_MODULES` (or `BOOT_MODULES`/`EXTRA_MODULES`) is enough for the generic `.ko` rule to build it.

### 6. Use in Your Kernel

In your example/kernel Makefile:

```makefile
KERNEL_MODULES = drivers/mymodule
```

In your code:

```c
#include <drivers/mymodule.h>

int kernel_start() {
    mymodule_init();
    printf("Value: %d\n", mymodule_get_value());
    return 0;
}
```

## Module Patterns

### Constructor/Destructor

Use GCC attributes for automatic initialization:

```c
#include <attrib.h>

constructor static void my_init(void) {
    // Called before main/kernel_start
}

destructor static void my_cleanup(void) {
    // Called at exit
}
```

### Weak Symbols

Provide default implementations that can be overridden:

```c
#include <attrib.h>

weak void my_handler(void) {
    // Default implementation
}
```

### Callbacks

Common pattern for event notification:

```c
static void (*user_callback)(int event);

void mymodule_set_callback(void (*cb)(int)) {
    user_callback = cb;
}

static void internal_event(int event) {
    if (user_callback)
        user_callback(event);
}
```

## Dependencies Between Modules

Modules can depend on other modules by using their symbols. The build system handles this automatically for static linking.

For dynamic loading with `complex-loader`, read all mutually dependent files into the same linkset before calling `loader_finish_link()`. This is what `bootloaders/elf-symbol/boot.c` does with the kernel payload (`elf-init`, `KERNEL_MODULES`, program); `EXTRA_MODULES` are then read and linked one at a time against the same linkset:

```c
struct linkset *linkset = loader_create_linkset();
loader_read_file(linkset, uart_file, "uart.ko");
loader_read_file(linkset, mux_file, "mux.ko");
loader_read_file(linkset, kernel_file, "kernel.ko");
if (loader_finish_link(linkset) != LOADER_ERROR_NONE)
    return -1;
void *entry = loader_search_symbol(linkset, "_start");
```

## Thread-Local Storage (TLS)

The `complex-loader` module supports TLS for modules that need per-thread data:

```c
__thread int my_tls_var = 0;  // Thread-local variable

void my_function(void) {
    my_tls_var++;  // Each thread has its own copy
}
```

TLS requires:
- The `elf-symbol` or `elf-uart` bootloader (or similar with `complex-loader` support)
- Proper TCB (Thread Control Block) setup via `loader_create_tcb(linkset)` and `loader_switch_tcb(tcb)`

## Build Output

After building, module artifacts are in:

```
build/
+-- modules/
    +-- arm/
    |   +-- irq.ko
    |   +-- mmu-basic.ko
    +-- drivers/
    |   +-- uart.ko
    |   +-- timer.ko
    |   +-- ...
    +-- sys/
    |   +-- mux.ko
    |   +-- printf.ko
    +-- complex-loader.ko
    +-- loader.ko
```

## Further Reading

- [Build System](../docs/build-system.md) — How modules are compiled and linked
- [Bootloaders](../bootloaders/README.md) — Different module loading strategies
- [ELF Loader Source](complex-loader/loader.c) — Implementation details
