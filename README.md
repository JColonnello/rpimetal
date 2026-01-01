# RPiMetal — SDK for baremetal development on Raspberry Pi 3

An out-of-the-box development environment for baremetal programming on a physical or emulated Raspberry Pi 3B+. Designed primarily for use in education — to learn about kernel and operative systems and as an Arduino-like platform

This project contains:

* A Docker image with all required tools preinstalled, including:
	* GCC
	* Binutils + GDB with Python support
	* Pre-built Libraries such as libc (Newlib), libm, libz and libbfd
	* clangd as language server (and [bear](https://github.com/rizsotto/Bear) to interface with the build system)
	* Python
	* [More programs and utilities...](toolchain/Dockerfile)
* A preconfigured VS Code workspace, the recommended IDE to use for this project
	* Devcontainer configuration
	* Preinstalled extensions
	* Debugger support
	* Custom terminal layouts
* Bootloaders with:
	* ELF support
	* Thread-local storage support
	* Multiple ways to deploy the software to a physical device
* Kernel modules implementing various drivers and functionalities
	* Interrupt handling
	* Framebuffer (display)
	* UART
	* SD card
	* FAT32
	* Mailbox interface
	* Generic ARM timer and BCM2837 local timer
	* ELF loader and one-way linker
* A client and server for a custom multiplexer protocol to use over UART
* An extensible build system using GNU Make
* Multiple example programs

## Installation

Clone the repo and open as devcontainer. Use image from dockerhub or build locally. Refer to [.devcontainer/devcontainer.json](.devcontainer/devcontainer.json) for details.

TODO: Write full instructions.

## Getting started

To test that everything is working, build the project and run the default example in QEMU:

1. In VS Code, open the project as a dev container
1. In the "Terminal" tab you should see the program output (and be able to type input depending on the example)
<!-- 1. Build the project using the "Build" task (Ctrl+Shift+B)
1. In the "QEMU" terminal tab, run `make run-vnc` -->

### Configuration

To change the program to build and run, create a `config.mk` file based on the provided `config.example.mk` file

```
cp config.example.mk config.mk
```

Only two variables need to be set:

* KERNEL: The kernel/program to run
* BOOTLOADER: The bootloader to use to load the KERNEL

For more information, follow the [tutorial](examples/README.md) (TODO)

### Terminal

Two terminal modes are supported: plain (or unmuxed) and muxed. In plain mode the program communicates through UART in plain ASCII text; in muxed mode the communication is multiplexed, allowing multiple channels of communication over a single UART connection.

Usually, a program is written to use either plain or muxed mode, and the active _terminal session_ should match the mode used. To switch between terminal sessions, use the 'Terminal Keeper' extension found in the primary side bar and select which one to use.

To learn more about terminal sessions and muxed communication, read the [multiplexing documentation](docs/multiplexing.md) TODO