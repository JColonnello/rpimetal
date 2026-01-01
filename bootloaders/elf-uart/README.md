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
- **Protocol support**: Compatible with existing upload tools (e.g., raspbootin-style)
- **Fallback**: Load default kernel if no upload within timeout
- **Checksum verification**: Ensure file integrity

## Current State

This bootloader is a copy of `elf-symbol` and is being modified to support UART loading. The core ELF loading infrastructure is in place; the serial receive protocol needs to be implemented.

## Files

### boot.S

Same as `elf-symbol/boot.S` — handles CPU initialization and EL2→EL1 transition.

### boot.c

Will be modified to:
1. Initialize UART
2. Wait for incoming data or timeout
3. Receive kernel ELF over serial
4. Load using the existing `loader` module
5. Jump to kernel

### linker.ld

Same as `elf-symbol/linker.ld`.

### Makefile

Same as `elf-symbol/Makefile` but may exclude embedded kernel payload.

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

On host:

```bash
# Build kernel
make

# Upload over serial
./upload-tool /dev/ttyUSB0 build/kernel.ko
```

## Related Projects

This approach is inspired by:
- [raspbootin](https://github.com/mrvn/raspbootin) — Serial bootloader for Raspberry Pi
- [bootloader tutorials in raspi3-tutorial](https://github.com/bztsrc/raspi3-tutorial)

## Contributing

If you'd like to help complete this bootloader, the main tasks are:

1. Implement serial receive protocol in `boot.c`
2. Create host-side upload tool
3. Add timeout and fallback behavior
4. Test with various kernel sizes

See the project's contribution guidelines for more information.
