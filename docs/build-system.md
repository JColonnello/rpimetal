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
+-- Makefile              # Main build file
+-- config.mk             # User configuration (create from config.example.mk)
+-- config.example.mk     # Example configuration
+-- build/                # Build artifacts (generated)
|   +-- kernel8.elf       # Linked ELF executable
|   +-- kernel.ko         # Program + modules combined (bootloaders/linked only)
|   +-- modules/          # Compiled modules
|   +-- bootloaders/      # Compiled bootloaders
|   +-- examples/         # Compiled programs, e.g. examples/display.ko
|   +-- extra/            # elf-init.ko (from extra/elf-init)
|   +-- payloads/         # Embedded payload objects (bootloaders/elf-symbol only)
|   +-- file-list.txt     # Files to send over UART (bootloaders/elf-uart only)
+-- output/               # Final outputs
|   +-- kernel8.img       # Binary image for Raspberry Pi
+-- modules/              # Reusable kernel modules
|   +-- Makefile          # Module list definitions
+-- bootloaders/          # Available bootloaders
+-- examples/             # Example programs
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

# Additional modules to include (space-separated list, commented out by default).
# Only used by the elf-symbol and elf-uart bootloaders; ignored by linked.
# EXTRA_MODULES = testing/test

# Remote server for deployment (optional)
RSYNC_SERVER = 192.168.0.199
```

If `config.mk` doesn't exist, the build system falls back to `config.example.mk`.

The program is compiled into `build/$(KERNEL).ko` (e.g. `build/examples/no-libc-uart.ko`). `EXTRA_MODULES` can also be set in the program's own Makefile (see `examples/linking/Makefile`).

### Using predefined Make variables

When writing example or module `Makefile`s you can leverage several
predefined variables and helper recipes exposed by the top-level
`Makefile`. These make it easy to refer to module source files and
output paths without hardcoding directory names.

- `DIR`: the relative path of the current module/program directory (e.g. `modules/drivers/uart`, `examples/display`)
- `BUILD_DIR`: the top-level build output directory (`build`)
- `OUTPUT_DIR`: the per-module output directory (`$(BUILD_DIR)/$(DIR)`)
- `OUTPUT_KO`: the resulting relocatable object for the module (`$(OUTPUT_DIR).ko`)
- `SOURCE_FILES`: all C/assembly source files in the module directory
- `OBJ_FILES`: the corresponding object filenames under `$(BUILD_DIR)` (used by the generic `.ko` recipe)

Example snippet you can reuse in an example `Makefile`:

```makefile
# KERNEL_MODULES defined by the example top-level Makefile
KERNEL_MODULES := drivers/uart sys/mux

# The top-level build system sets DIR and BUILD_DIR for you. Use
# `OUTPUT_DIR` and `OUTPUT_KO` to refer to build outputs.
$(OUTPUT_KO): $(OBJ_FILES)

# Use SOURCE_FILES to add extra build steps or to inspect which
# sources will be compiled for the current module.
$(info Building module $(DIR) with sources: $(SOURCE_FILES))
```

The top-level `Makefile` also defines empty/default recipes and object
rules so most modules require no special Makefile content — only
module-specific flags or extra link libraries when necessary.

## Key Make Targets

| Target | Description |
|--------|-------------|
| `make` or `make all` | Build `kernel8.img` and the SD card image |
| `make clean` | Remove all build artifacts |
| `make rebuild` | Clean and build |
| `make run` | Build and run in QEMU (serial on TCP) |
| `make run-vnc` | Build and run in QEMU with VNC display |
| `make run-inline` | Build and run in QEMU with serial on stdio (and VNC display) |
| `make uart0` | Listen on TCP port 4444 for plain serial output (`nc -lkvp 4444`) |
| `make debug` | Build and run in QEMU, waiting for debugger |
| `make debug-vnc` | Same as above, with VNC display |
| `make undef` | Show undefined symbols in `$(BUILD_DIR)/$(KERNEL).ko` (the program `.ko`) |
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
STD_MODULES = testing/test testing/test2 drivers/uart drivers/mbox loader arm/irq arm/mmu-basic drivers/display drivers/timer \
sys/mux drivers/sd drivers/sd2 dummy sys/printf drivers/simple-uart fs/fat complex-loader

# Multi-file modules (may have dependencies)
MULTI_MODULES = testing/test3 libc
```

These lists only register module Makefiles: the top-level Makefile does `-include` on `modules/<name>/Makefile` for each `STD_MODULES` entry and `include` for each `MULTI_MODULES` entry. Any module named in `KERNEL_MODULES`, `BOOT_MODULES` or `EXTRA_MODULES` is built by the generic `$(BUILD_DIR)/%.ko` rule, except multi-modules, which must provide their own rules.

Each module folder contains:
- Source files (`.c`, `.S`)
- Optionally, a `Makefile` with module-specific settings (e.g. `modules/complex-loader/Makefile` adds `LDLIBS`). Most modules don't have one.
- The build produces a `.ko` file in `build/modules/`

### 3. Building a Kernel

When you run `make`, the following happens:

1. **Include configuration**: Load `config.mk` or `config.example.mk`
2. **Include module Makefiles**: Process `modules/Makefile` and each module's Makefile
3. **Compile sources**: `.c` → `.c.o`, `.S` → `.S.o`
4. **Link modules**: Object files → `.ko` relocatable objects
5. **Link `kernel8.elf`**: depends on the bootloader (see below)
6. **Create image**: `objcopy` → `kernel8.img`

How `kernel8.elf` is produced depends on `BOOTLOADER`:
- `bootloaders/linked`: `elf-init.ko`, the `KERNEL_MODULES` and the program `.ko` are combined into `build/kernel.ko`, which is linked with the bootloader sources and `BOOT_MODULES` into a single executable.
- `bootloaders/elf-symbol`: the bootloader and its `BOOT_MODULES` are linked into `kernel8.elf`, with `elf-init.ko`, `KERNEL_MODULES` and the program `.ko` embedded as `build/payloads/kernel_payload.o`, and `EXTRA_MODULES` as `build/payloads/extra_payload.o`. The `complex-loader` module links them at runtime.
- `bootloaders/elf-uart`: only the bootloader and its `BOOT_MODULES` are linked. The kernel files are sent over UART at runtime, following `build/file-list.txt` (kernel-phase files, a blank line, then `EXTRA_MODULES`).

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

`run-inline` uses `-serial stdio` instead of `-serial tcp:localhost:4444`, so serial I/O goes to the current terminal and no listener is needed. The other run/debug targets connect to `localhost:4444` as a client, so start a listener first (`make uart0` for plain mode, `make mux-tcp` for muxed mode).

For automated or agent runs, `./inline.sh [timeout]` starts the multiplexer on port 4444, runs `make run-vnc` in the background, prints the muxed stdout/stderr channels in the current terminal and kills QEMU after `timeout` seconds (default 5).

Key QEMU options:
- `-M raspi3b`: Emulate Raspberry Pi 3B
- `-kernel`: Load our kernel8.img
- `-serial tcp:localhost:4444`: Serial port on TCP
- `-drive`: Attach SD card image
- `-vnc :1`: VNC server on display :1 (port 5901)
- `-S -s`: (debug mode) Stop CPU and enable GDB server on :1234
