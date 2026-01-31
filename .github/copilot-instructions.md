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
    - `output/` — final artifacts (e.g. `kernel8.img`, `multiplex`).
    - `docs/` — documentation
    - `remote-server/` — Docker Compose setup for PXE network boot and deployment to real Raspberry Pi hardware.
    - `include/` — public headers for each module, and other shared definitions.
    - `toolchain/` — Dockerfile and scripts to build a cross-compilation toolchain image. Used as a devcontainer in VSCode.

- How the build works (short)
  - Top-level `Makefile` includes `config.mk` (user configuration), module lists and then the bootloader and program (usually an example) makefiles.
  - `[program]/Makefile` compiles source into `$(BUILD_DIR)/[program].ko`, then `kernel.ko`.
  - `[bootloader]/Makefile` links `kernel8.elf` using `linker.ld` and payload modules; `objcopy` turns it into `output/kernel8.img`.
  - Modules are built as relocatable objects (`.ko`) via `-r` (partial linking).

- Common developer workflows & commands (copyable)
  - Build everything: `make all` (at repo root). Already configured as default build task in VSCode.
  - Run in QEMU: `make run-vnc`
  - Debug (waits for gdb): `make debug-vnc` (adds `-S -s` to qemu). The default debug task already starts QEMU and gdb, no extra commands needed.
  - Open serial console (in plain mode): `nc -lkvp 4444`. Already open in VSCode terminal named "Terminal"
  - Open serial console (in mux mode): `nc -lkvp 4440` and `nc -lkvp 4441`. Already open in VSCode terminals named "stdout" and "stderr"
  - Multiplex serial to TCP: `make mux-tcp` (uses `socat` and `output/multiplex` with `config-mult.txt`). Already running when switching to "muxed" session in 'Terminal Keeper' extension.

- Debugging notes
  - Serial I/O is exposed on TCP port 4444 — tests and interactive sessions use `nc` or `socat`. When using muxing, the muxer reads from port 4444 and connects to multiple clients.
  stdout/stdin is usually sent in port 4440 and stderr in 4441.

- Project-specific conventions & patterns
  - Modules pattern: module folders live in `modules/<name>/`. A module appears in build via entries in top-level module lists, the `examples` Makefile's `KERNEL_MODULES` variable, or the `bootloaders/` Makefile's `BOOT_MODULES` variable.
  - `.ko` files in `build/` are not Linux kernel modules — they are relocatable object ELF files used to compose the final image.
  - Assembly: `.S` and `.s` files are used for low-level startup or IRQ handling (see `boot.S` and `modules/arm/irq/irq.S`).

- Integration points & external dependencies
  - Runtime emulation: `qemu-system-aarch64` (required to run/debug images).
  - Networking/tools: `nc`, `socat` for serial multiplexing.
  - `compile_commands.json` is present to support language servers / code navigation.

- How to get help / more info
  - Consult `README.md` and follow links to other relevant '.md' files (primarily in `docs/`)

- How to ask the human for help (when uncertain)
  - If build fails: paste `make` output and the failing compile/link command from the top-level `make` run.
  - For runtime issues: provide the `qemu` command line (visible in `Makefile`), the serial log (port 4444), and the exact image under `output/`.

If anything here is incomplete or more documentation is needed (e.g., step-by-step to add a new module), tell the human which area to expand and they will (eventually) update this file.
