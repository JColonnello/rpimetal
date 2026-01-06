# RPiMetal — SDK for baremetal development on Raspberry Pi 3

An out-of-the-box development environment for baremetal programming on a physical or emulated Raspberry Pi 3B+. Designed primarily for use in education — to learn about kernel and operating systems development, and as an Arduino-like platform for low-level programming.

## Features

* **Docker-based toolchain** with all required tools preinstalled:
  * GCC cross-compiler (`aarch64-none-elf`)
  * Binutils + GDB with Python support
  * Pre-built libraries: libc (Newlib), libm, libz, and libbfd
  * clangd language server (with [bear](https://github.com/rizsotto/Bear) for build system integration)
  * QEMU for emulation
  * [Full list of tools...](toolchain/Dockerfile)

* **Preconfigured VS Code workspace** (recommended IDE):
  * Dev Container configuration for instant setup
  * Preinstalled extensions for C/C++ and ARM development
  * Integrated debugger support
  * Custom terminal layouts for QEMU and serial communication

* **Multiple bootloaders** with different capabilities:
  * Static linking (simplest approach)
  * Dynamic ELF loading with runtime symbol resolution
  * Thread-local storage (TLS) support
  * [Bootloader documentation](bootloaders/README.md)

* **Modular kernel components**:
  * Interrupt handling (IRQ)
  * Memory Management Unit (MMU) setup
  * UART driver (PL011)
  * Framebuffer display driver
  * SD card and FAT32 filesystem
  * Mailbox interface for GPU communication
  * ARM generic timer and BCM2837 local timer
  * ELF loader with libbfd-based symbol resolution
  * [Module documentation](modules/README.md)

* **Serial multiplexing protocol** for multi-channel communication over UART
  * [Multiplexing documentation](docs/multiplexing.md)

* **Extensible GNU Make build system**
  * [Build system documentation](docs/build-system.md)

* **Tutorial examples** with progressive complexity
  * [Example tutorials](examples/README.md)

## Installation

### Prerequisites

* [Docker](https://www.docker.com/get-started) installed and running
* [Visual Studio Code](https://code.visualstudio.com/) with the [Dev Containers extension](https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.remote-containers)
* Git

### Setup

1. Clone the repository:
   ```bash
   git clone https://github.com/your-username/rpimetal.git
   cd rpimetal
   ```

2. Open in VS Code:
   ```bash
   code .
   ```

3. When prompted, click **"Reopen in Container"** (or use the command palette: `Dev Containers: Reopen in Container`). This will:
   * Pull the `jcolonnello/rpimetal` Docker image (or build it locally)
   * Mount the workspace inside the container
   * Install recommended VS Code extensions

4. Wait for the container to build and start. The first time may take a few minutes.

### Building the Toolchain Locally (Optional)

If you prefer to build the Docker image locally instead of pulling from Docker Hub:

```bash
make toolchain
```

Then modify `.devcontainer/devcontainer.json` to use the local image:
```json
"build": {
    "dockerfile": "../toolchain/Dockerfile"
}
```

## Getting Started

Once the dev container is running, test that everything works:

### 1. Build and Run

The project should build automatically when the container starts. You can also build manually:

```bash
make
```

To run in QEMU with VNC display:
```bash
make run-vnc
```

Or to use a graphical window for display:
```bash
make run
```

### 2. View Output

* **Serial output**: Open a terminal and run `nc localhost 4444` (or use the preconfigured terminal tabs)
* **VNC display**: Connect to `localhost:5901` with a VNC client, or use the VS Code Simple Browser

### 3. Debug

Use the VS Code debugger (F5) with the 'Debug (VNC)' configuration.

By default the VS Code debug task launches the server and starts QEMU
for you; running `make debug-vnc` is optional unless you need to start
QEMU manually or require an explicit VNC invocation. To connect GDB
manually run:

```bash
aarch64-none-elf-gdb build/kernel8.elf -ex "target remote :1234"
```

## Configuration

To change which program to build and run, create a `config.mk` file:

```bash
cp config.example.mk config.mk
```

Edit `config.mk` to set:

```makefile
# The kernel/program to build and run
KERNEL = examples/no-libc-uart

# The bootloader to use
BOOTLOADER = bootloaders/linked

# Optional: additional modules to include
# EXTRA_MODULES = testing/test
```

### Available Bootloaders

| Bootloader | Description | Use Case |
|------------|-------------|----------|
| `bootloaders/linked` | Statically links kernel with startup code | Simple programs, no dynamic loading |
| `bootloaders/elf-symbol` | Loads kernel as ELF, resolves symbols at runtime | Programs using modules, dynamic features |
| `bootloaders/elf-uart` | (WIP) Loads ELF via UART | Development without SD card reflashing |

See [bootloaders/](bootloaders/) for detailed documentation on each.

## Terminal Modes

Two terminal modes are supported:

### Plain Mode (Unmuxed)
The program communicates through UART in plain ASCII text. Use this for simple programs that only need stdin/stdout.

### Muxed Mode
Communication is multiplexed, allowing multiple channels over a single UART connection. This enables separate channels for stdout, stderr, debug output, file transfer, etc.

To switch between terminal sessions, use the **Terminal Keeper** extension in the VS Code sidebar.

For protocol details and implementation guide, see [docs/multiplexing.md](docs/multiplexing.md).

## Architecture Overview

RPiMetal follows a modular architecture where the final kernel image is composed of:

1. **Bootloader**: Initializes the CPU, sets up the stack, and transfers control to the kernel. Different bootloaders support different loading strategies (static linking vs. dynamic ELF loading).

2. **Kernel/Program**: Your application code, compiled as a relocatable object (`.ko` file).

3. **Modules**: Reusable components (drivers, libraries) that can be linked statically or loaded dynamically depending on the bootloader used.

### Boot Process

The Raspberry Pi boot sequence involves multiple stages:

1. The GPU loads `bootcode.bin` from the SD card
2. `bootcode.bin` loads `start.elf`, which reads `config.txt`
3. `start.elf` loads `kernel8.img` (our image) at address `0x80000` for AArch64
4. The ARM CPU starts executing our bootloader code
5. The bootloader initializes the system and jumps to the kernel entry point

### Memory Layout

```
0x00000000 - 0x00080000  : Reserved (GPU, interrupt vectors)
0x00080000 - 0x????????  : Kernel image (bootloader + kernel + modules)
0x???????? - 0x3E000000  : Available RAM for heap/dynamic allocation
0x3E000000 - 0x40000000  : Reserved (GPU memory, MMIO)
0x40000000+              : Peripheral registers (MMIO)
```

## Documentation

* [Build System](docs/build-system.md) — How the Makefile works, adding modules
* [Multiplexing Protocol](docs/multiplexing.md) — Serial multiplexing for multi-channel I/O
* [Modules](modules/README.md) — Available kernel modules and how to create new ones
* [Hardware deployment](docs/hardware-deploy.md) — Instructions for running on real hardware
* [Bootloaders](bootloaders/README.md) — Different bootloader options and their use cases
* [Examples/Tutorials](examples/README.md) — Step-by-step tutorials from simple to complex

## Quick Reference

| Command | Description |
|---------|-------------|
| `make` | Build the kernel image |
| `make clean` | Remove build artifacts |
| `make run` | Run in QEMU (serial only) |
| `make run-vnc` | Run in QEMU with VNC display |
| `make debug` | Run in QEMU, wait for debugger |
| `make debug-vnc` | Run in QEMU with VNC, wait for debugger |
| `make mux-tcp` | Start the serial multiplexer |
| `make undef` | Show undefined symbols in kernel |

## License

This repository is licensed under the GNU Lesser General Public License
v3.0 or later. See the `LICENSE` file at the project root for the
full text.

## Acknowledgments

This project was inspired by and references several excellent bare-metal and OS development resources:

* [raspberry-pi-os](https://github.com/s-matyukevich/raspberry-pi-os) by Sergey Matyukevich
* [raspi3-tutorial](https://github.com/bztsrc/raspi3-tutorial) by Zoltan Baldaszti
* [Circle](https://github.com/rsta2/circle) by Rene Stange
* [rpi-boot](https://github.com/jncronin/rpi-boot) by jncronin