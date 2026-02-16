<!-- Copilot / AI agent instructions for the rpimetal repo -->
# rpimetal — AI Agent Guidance

Purpose: give a compact, repo-specific summary so an AI code agent can be immediately productive.

- Big picture
  - This repo contains a bare-metal OS development framework—including tools, libraries (modules), and related code—for building bare-metal applications on Raspberry Pi devices (primarily RPi 3B+).
  - Key components:
    - `bootloaders/` — boot image/linker, startup (`boot.S`, `boot.c`, `linker.ld`).
    - `examples/` — reference kernel/application examples. It demonstrates how modules and application logic are combined into the kernel image `kernel.ko`.
    - `modules/` — reusable modules (drivers, libc, loader, testing). Each module has a subfolder and may have its own `Makefile` (to tweak the build recipes).
    - `build/` — ephemeral build artifacts (object files, `.ko`, `kernel8.elf`). Do not edit directly.
    - `output/` — final artifacts (e.g. `kernel8.img`).
    - `docs/` — documentation
    - `remote-server/` — Docker Compose setup for PXE network boot and deployment to real Raspberry Pi hardware.
    - `include/` — public headers for each module, and other shared definitions.
    - `toolchain/` — Dockerfile and scripts to build a cross-compilation toolchain image. Used as a devcontainer in VSCode.

- How the build works (short)
  - Top-level `Makefile` includes `config.mk` (user configuration), module lists and then the bootloader and program (usually an example) makefiles.
  - `[program]/Makefile` compiles source into `$(BUILD_DIR)/[program].ko`.
  - `[bootloader]/Makefile` links `kernel8.elf` using `linker.ld` and payload modules; `objcopy` turns it into `output/kernel8.img`.
  - Modules are built as relocatable objects (`.ko`) via `-r` (partial linking).

- Common developer workflows & commands (copyable)
  - Build everything: `make all` (at repo root). Already configured as default build task in VSCode.
  - Run in QEMU inline (for agents): `./inline.sh [timeout]`. The command timeouts after 5 seconds by default. Other ways to make sure the agent doesn't get stuck in an infinite loop:
    * Pipe it into other commands that terminate on their own (e.g., `grep -m1`)
    * Have the program under test trigger a shutdown after its logic runs (see `examples/linking/stdlib.c` for an example of how to do this via the mailbox).

- Project-specific conventions & patterns
  - Modules pattern: module folders live in `modules/<name>/`. A module appears in build via entries in top-level module lists, the `examples` Makefile's `KERNEL_MODULES` variable, or the `bootloaders/` Makefile's `BOOT_MODULES` variable.
  - `.ko` files in `build/` are not Linux kernel modules — they are relocatable object ELF files used to compose the final image.
  - Assembly: `.S` and `.s` files are used for low-level startup or IRQ handling (see `boot.S` and `modules/arm/irq/irq.S`).

- Integration points & external dependencies
  - Runtime emulation: `qemu-system-aarch64` (required to run/debug images).
  - Networking/tools: `nc`, `socat` for serial multiplexing.

- How to get help / more info
  - Consult `README.md` and follow links to other relevant '.md' files (primarily in `docs/`)

- How to ask the human for help (when uncertain)
  - If build fails: paste `make` output and the failing compile/link command from the top-level `make` run.
  - For runtime issues: provide the testing command run and the serial log output (from QEMU or real hardware).

If anything here is incomplete or more documentation is needed (e.g., step-by-step to add a new module), tell the human which area to expand and they will (eventually) update this file.
