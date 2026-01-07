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

2. **Dynamic loading** (with `bootloaders/elf-symbol`): Modules are loaded at runtime by the ELF loader, with symbols resolved dynamically using libbfd.

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
| `drivers/mbox` | Mailbox interface for GPU communication. Used to query/set system properties. |
| `drivers/display` | Framebuffer driver. Sets up screen resolution, provides `lfb_showpicture()` for drawing. |
| `drivers/sd` | SD card driver (EMMC interface). Low-level block read/write. |
| `drivers/sd2` | Alternative SD card implementation. |

### sys/ — System Services

System-level utilities and services.

| Module | Description |
|--------|-------------|
| `sys/mux` | Serial multiplexing. Enables multiple channels over single UART. See [multiplexing.md](../docs/multiplexing.md). |
| `sys/printf` | Lightweight printf implementation. Provides `init_printf()` and `printf()` functions. |

### fs/ — Filesystems

Filesystem implementations.

| Module | Description |
|--------|-------------|
| `fs/fat` | FAT32 filesystem using FatFs library. Requires `drivers/sd` or `drivers/sd2`. |

### loader/ — ELF Loader

The dynamic module loader.

| Module | Description |
|--------|-------------|
| `loader` | ELF loader using libbfd. Loads `.ko` files, resolves symbols, supports TLS. Used by `elf-symbol` bootloader. |

**Key functions:**
- `loader_init()` — Initialize the loader and libbfd
- `loader_load_file(file, name)` — Load an ELF module from a FILE*
- `loader_search_symbol(name)` — Find a symbol by name
- `loader_create_tcb()` — Create a Thread Control Block for TLS

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
STD_MODULES = drivers/uart drivers/mbox loader arm/irq arm/mmu-basic \
              drivers/display drivers/timer sys/mux drivers/sd drivers/sd2 \
              dummy sys/printf drivers/simple-uart fs/fat

# Multi-file modules (may span multiple source files or have special build rules)
MULTI_MODULES = testing/test3 libc
```

## Creating a New Module

### 1. Create the Module Directory

```
modules/
└── mymodule/
    ├── mymodule.c
    └── Makefile
```

### 2. Write the Source Code

```c
// modules/mymodule/mymodule.c
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

### 4. Create the Module Makefile

```makefile
# modules/mymodule/Makefile

# Optional: Additional libraries to link
# LDLIBS = -lsomelib

# Optional: Additional compiler flags
# CFLAGS += -DSOME_DEFINE
```

Most modules need no special Makefile content — the default rules handle compilation.

### 5. Register the Module

Add to `modules/Makefile`:

```makefile
STD_MODULES = ... mymodule
```

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

For dynamic loading, ensure dependencies are loaded first:

```c
// In bootloader boot.c
loader_load_file(uart_file, "uart.ko");    // Load dependency first
loader_load_file(mux_file, "mux.ko");      // mux depends on uart
loader_load_file(kernel_file, "kernel.ko"); // kernel uses both
```

## Thread-Local Storage (TLS)

The loader module supports TLS for modules that need per-thread data:

```c
__thread int my_tls_var = 0;  // Thread-local variable

void my_function(void) {
    my_tls_var++;  // Each thread has its own copy
}
```

TLS requires:
- The `elf-symbol` bootloader (or similar with loader support)
- Proper TCB (Thread Control Block) setup via `loader_create_tcb()`

## Build Output

After building, module artifacts are in:

```
build/
└── modules/
    ├── arm/
    │   ├── irq.ko
    │   └── mmu-basic.ko
    ├── drivers/
    │   ├── uart.ko
    │   ├── timer.ko
    │   └── ...
    ├── sys/
    │   ├── mux.ko
    │   └── printf.ko
    └── loader/
        └── loader.ko
```

## Further Reading

- [Build System](../docs/build-system.md) — How modules are compiled and linked
- [Bootloaders](../bootloaders/README.md) — Different module loading strategies
- [ELF Loader Source](loader/loader.c) — Implementation details
