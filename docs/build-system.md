# Build System

RPiMetal uses GNU Make for its build system. This document explains how the build works, the key configuration options, and how to extend it with new modules or examples.

## Overview

The build system compiles source code into **relocatable object files** (`.ko` files), which are then linked together by the bootloader to create the final `kernel8.img` that runs on the Raspberry Pi.

```
Source files (.c, .S)
        ↓
Object files (.o)
        ↓
Relocatable objects (.ko)
        ↓
Linked ELF (kernel8.elf)
        ↓
Binary image (kernel8.img)
```

## Directory Structure

```
rpimetal/
├── Makefile              # Main build file
├── config.mk             # User configuration (create from config.example.mk)
├── config.example.mk     # Example configuration
├── build/                # Build artifacts (generated)
│   ├── kernel.ko         # Compiled kernel
│   ├── kernel8.elf       # Linked ELF executable
│   ├── modules/          # Compiled modules
│   ├── bootloaders/      # Compiled bootloaders
│   └── examples/         # Compiled examples
├── output/               # Final outputs
│   └── kernel8.img       # Binary image for Raspberry Pi
├── modules/              # Reusable kernel modules
│   └── Makefile          # Module list definitions
├── bootloaders/          # Available bootloaders
└── examples/             # Example programs
```

## Configuration

### config.mk

Create your configuration by copying the example:

```bash
cp config.example.mk config.mk
```

The main variables are:

```makefile
# The kernel/program to build (path to folder containing source and Makefile)
KERNEL = examples/no-libc-uart

# The bootloader to use (path to bootloader folder)
BOOTLOADER = bootloaders/linked

# Additional modules to include (space-separated list)
EXTRA_MODULES = testing/test

# Remote server for deployment (optional)
RSYNC_SERVER = 192.168.0.199
```

If `config.mk` doesn't exist, the build system falls back to `config.example.mk`.

## Key Make Targets

| Target | Description |
|--------|-------------|
| `make` or `make all` | Build `kernel8.img` and the SD card image |
| `make clean` | Remove all build artifacts |
| `make rebuild` | Clean and build |
| `make run` | Build and run in QEMU (serial on TCP) |
| `make run-vnc` | Build and run in QEMU with VNC display |
| `make debug` | Build and run in QEMU, waiting for debugger |
| `make debug-vnc` | Same as above, with VNC display |
| `make undef` | Show undefined symbols in kernel.ko |
| `make mux-tcp` | Start the serial multiplexer |
| `make sync` | Rsync output/ to remote server |
| `make toolchain` | Build the Docker toolchain image locally |

## How the Build Works

### 1. Toolchain Configuration

The Makefile sets up the cross-compiler toolchain:

```makefile
TRIPLET = aarch64-none-elf
ARMGNU = /opt/$(TRIPLET)/bin/$(TRIPLET)
CC := $(ARMGNU)-gcc
LD := $(ARMGNU)-ld
AS := $(ARMGNU)-gcc
```

Key compiler flags:
- `-march=armv8-a -mtune=cortex-a53`: Target ARM Cortex-A53 (RPi 3)
- `-mtp=el1`: Thread pointer register for EL1
- `-nolibc`: Don't link standard C library automatically
- `-ftls-model=local-exec`: Thread-local storage model

### 2. Module System

Modules are defined in `modules/Makefile`:

```makefile
# Standard modules (compiled individually)
STD_MODULES = drivers/uart drivers/mbox loader arm/irq ...

# Multi-file modules (may have dependencies)
MULTI_MODULES = testing/test3 libc
```

Each module folder contains:
- Source files (`.c`, `.S`)
- A `Makefile` that defines module-specific settings
- The build produces a `.ko` file in `build/modules/`

### 3. Building a Kernel

When you run `make`, the following happens:

1. **Include configuration**: Load `config.mk` or `config.example.mk`
2. **Include module Makefiles**: Process `modules/Makefile` and each module's Makefile
3. **Compile sources**: `.c` → `.c.o`, `.S` → `.S.o`
4. **Link modules**: Object files → `.ko` relocatable objects
5. **Link kernel**: Bootloader + kernel + modules → `kernel8.elf`
6. **Create image**: `objcopy` → `kernel8.img`

### 4. Relocatable Objects (.ko)

The `.ko` files are **not** Linux kernel modules — they are relocatable ELF objects created with:

```makefile
$(CC) -r $(OBJ_FILES) $(LDLIBS) -o $@
```

The `-r` flag produces relocatable output that can be linked again later. This allows:
- Modules to be combined in different configurations
- Dynamic loading at runtime (with `elf-symbol` bootloader)
- Symbol resolution to happen at link time or load time

## Dependency Tracking

The build system automatically tracks dependencies using GCC's `-MMD` flag, which generates `.d` files:

```makefile
$(BUILD_DIR)/%.c.o : %.c
	$(CC) $(CFLAGS) $(INC_FLAGS) -MMD -c $< -o $@
```

These `.d` files are included at the end of the Makefile:
```makefile
include $(shell find $(BUILD_DIR) -name '*.d')
```

This ensures that changing a header file triggers recompilation of all affected source files.

## Debugging Build Issues

### View Compilation Commands

To see the exact commands being run:
```bash
make V=1
```

### Check Undefined Symbols

To see what symbols are undefined in your kernel:
```bash
make undef
```

This helps identify missing module dependencies.

### Generate Compilation Database

For clangd/IDE support, regenerate `compile_commands.json`:
```bash
bear --append -- make -r -j8 all
```

This is done automatically by the VS Code build task.

## QEMU Integration

The Makefile includes QEMU targets for testing:

```makefile
run-vnc: all
	qemu-system-aarch64 -M raspi3b \
		-kernel $(IMAGE) \
		-serial tcp:localhost:4444 \
		-drive file=$(SD),if=sd,format=raw \
		-vnc :1,websocket=on
```

Key QEMU options:
- `-M raspi3b`: Emulate Raspberry Pi 3B
- `-kernel`: Load our kernel8.img
- `-serial tcp:localhost:4444`: Serial port on TCP
- `-drive`: Attach SD card image
- `-vnc :1`: VNC server on display :1 (port 5901)
- `-S -s`: (debug mode) Stop CPU and enable GDB server on :1234
