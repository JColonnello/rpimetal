# Bootloader: elf-uart

**Status: Work in Progress**

A bootloader that loads the kernel ELF file over the UART serial connection, eliminating the need to reflash the SD card during development.

## Overview

This bootloader is similar to `elf-symbol` but instead of loading the kernel from embedded binary data, it receives the kernel over the serial port. This enables rapid development cycles:

1. Write code
2. Compile
3. Send over serial
4. Run immediately

No need to:
- Write to SD card
- Swap SD card between computer and Raspberry Pi
- Wait for SD card write operations

## Planned Features

- **Serial upload**: Receive `.ko` files over UART
- **Protocol support**: Simple plain-text protocol (except file content) over a multiplexed channel
- **Fallback**: Load default kernel if no upload within timeout
- **Checksum verification**: Ensure file integrity
- **Fast transfer**: Save received files to the SD card and only transfer updated files (by checking timestamp and checksum)

## Current State

This bootloader is a copy of `elf-symbol` and is being modified to support UART loading. The core ELF loading infrastructure is in place; the serial receive protocol needs to be implemented.

## Files

### boot.S

Same as `elf-symbol/boot.S` — handles CPU initialization and EL2→EL1 transition.

### boot.c

Will be modified to:
1. Initialize UART
2. Negotiate with the host or timeout
3. Receive files over serial and save them
4. Load using the existing `complex-loader` module (like `elf-symbol`)
5. Jump to kernel

### linker.ld

Same as `elf-symbol/linker.ld`.

### Makefile

Same as `elf-symbol/Makefile` but without embedded kernel payload. Instead generates list of files required and uses it during negotiation

## Planned Protocol

The upload protocol will be simple:

1. Bootloader sends "ready" marker over serial
2. Host sends file size (4 bytes, little-endian)
3. Host sends file data
4. Bootloader sends acknowledgment or error
5. Bootloader jumps to loaded kernel

## Usage (Future)

In `config.mk`:

```makefile
BOOTLOADER = bootloaders/elf-uart
KERNEL = examples/my-program
```

In `config-mult.txt`:
```
0,nc localhost 4440
1,nc localhost 4441
-1,./upload-tool build/file-list.txt
```

On host:

```bash
# Build kernel
make

# Have muxer running
make mux-tcp
# Run it, the upload protocol runs through channel -1 automatically
make run-mux-inline
# Or any other variant
make run-vnc
```

## Related Projects

This approach is inspired by:
- [raspbootin](https://github.com/mrvn/raspbootin) — Serial bootloader for Raspberry Pi
- [bootloader tutorials in raspi3-tutorial](https://github.com/bztsrc/raspi3-tutorial)
