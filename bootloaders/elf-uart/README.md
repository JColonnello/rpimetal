# Bootloader: elf-uart

**Status: Work in Progress.** It works, but the upload is too slow for normal use.

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

## Current State

The upload protocol is implemented and the bootloader can receive, link and run the kernel over the serial port. It is still WIP because the transfer is too slow for normal use: every file goes through the mux channel on each boot.

Remaining gaps:
- **No timeout or fallback**: `recv_line()` waits forever for the host; there is no default kernel if no upload arrives
- **No file checksum**: data integrity relies only on the per-packet checksum of the mux protocol
- **No SD caching**: all files are sent on every boot, even if unchanged

## Files

### boot.S

Same as `elf-symbol/boot.S`: handles CPU initialization and EL2→EL1 transition.

### boot.c

`main()`:
1. Creates a linkset and registers the base symbols, like `elf-symbol`
2. Registers the upload channel with `mux_channel_add(-1, 65536, true)`
3. Sends `READY\n` on channel -1
4. Receives the kernel files, reads each one into the linkset with `fmemopen()` + `loader_read_file()`, and links them with `loader_finish_link()` when the separator arrives
5. Receives the extra modules, linking after each one, until `DONE`
6. Creates the TCB, fills `boot_info`/`boot_customdata` and resolves `_start`
7. Returns from `main()`; `_exit(0)` calls `kernel_jump()` (same as `elf-symbol`)

### linker.ld

Same as `elf-symbol/linker.ld`.

### Makefile

Same as `elf-symbol/Makefile` but without embedded payloads. Instead it generates `build/file-list.txt` with the files to upload, in this order:

```
build/extra/elf-init.ko
build/modules/<KERNEL_MODULES>.ko ...
build/$(KERNEL).ko

build/modules/<EXTRA_MODULES>.ko ...
```

The blank line separates the kernel files from the extra modules.

### upload-tool

Host-side shell script that reads the file list and sends the files over channel -1. It is started by the multiplexer (see `config-mult.txt` below), with its stdin/stdout connected to the channel.

## Protocol

Plain-text lines, except for the file contents, all over mux channel -1:

1. Bootloader sends `READY\n`
2. For each file, host sends `<path>\n<size in decimal>\n` followed by the raw file bytes. Bootloader replies `ACK\n`
3. Host sends an empty line (`\n`) to separate the kernel files from the extra modules. No `ACK` is sent for it. The bootloader links all kernel files at this point
4. Extra modules follow with the same format as step 2. The bootloader links after each one, then sends `ACK\n`
5. Host sends `DONE\n`. The bootloader jumps to the kernel

If a kernel file fails to load, the bootloader prints an error and sends no `ACK`. `upload-tool` keeps waiting for it, so the upload stalls.

## Usage

In `config.mk`:

```makefile
BOOTLOADER = bootloaders/elf-uart
KERNEL = examples/my-program
```

In `config-mult.txt`:
```
0,nc localhost 4440
1,nc localhost 4441
-1,./bootloaders/elf-uart/upload-tool build/file-list.txt
```

On host:

```bash
# Build kernel and file list
make

# Listeners for stdout (channel 0) and stderr (channel 1), each in its own terminal
nc -lkvp 4440
nc -lkvp 4441

# Have muxer running
make mux-tcp
# Run it, the upload protocol runs through channel -1 automatically
make run
# Or any other variant
make run-vnc
```

Or run everything inline with `./inline.sh [timeout]`, which starts the muxer with `config-mult.txt`, the listeners and `make run-vnc`. Pass a timeout long enough for the upload to finish (the default is 5 seconds).

## Related Projects

This approach is inspired by:
- [raspbootin](https://github.com/mrvn/raspbootin) — Serial bootloader for Raspberry Pi
- [bootloader tutorials in raspi3-tutorial](https://github.com/bztsrc/raspi3-tutorial)
