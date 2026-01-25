# Example Tutorials

This directory contains example programs that demonstrate RPiMetal's capabilities with progressive complexity. Each example is a standalone project that can be built and run independently.

## Tutorial Progression

Start with the simplest example and progress to more complex ones:

| # | Example | Description | Bootloader | Key Concepts |
|---|---------|-------------|------------|--------------|
| 1 | [no-libc-uart](no-libc-uart/README.md) | Minimal "Hello World" | `linked` | Basic UART output, simple-uart driver |
| 2 | [display](display/README.md) | Show an image on screen | `linked` | Framebuffer, mailbox, graphics |
| 3 | [0A_misc](0A_misc/README.md) | Interactive echo with timing | `linked` | Full libc, timer, stdin/stdout, interrupts |

## Quick Start

To run any example, edit `config.mk`:

```makefile
KERNEL = examples/no-libc-uart
BOOTLOADER = bootloaders/linked
```

Then build and run:

```bash
make
make run-vnc
```

## Example Structure

Each example follows a similar structure:

```
examples/
└── my-example/
    ├── kernel.c      # Main program code (entry point: kernel_start)
    ├── Makefile      # Defines KERNEL_MODULES used by this example
    └── *.c, *.h      # Additional source files (optional)
```

### The Makefile

Each example's Makefile primarily defines which modules it needs:

```makefile
# List of modules this example depends on
KERNEL_MODULES := drivers/simple-uart sys/printf
```

### The Entry Point

Your program should define `kernel_start()`:

```c
int kernel_start() {
    // Your code here
    return 0;
}
```

Or with boot information:

```c
int kernel_start(struct boot_info *info, union boot_userdata userdata) {
    // Access memory info, custom data
    return 0;
}
```

## Creating Your Own Example

1. Create a new directory:
   ```bash
   mkdir examples/my-example
   ```

2. Create `Makefile` with your module dependencies:
   ```makefile
   KERNEL_MODULES := drivers/simple-uart sys/printf
   ```

3. Create `kernel.c`:
   ```c
   #include <drivers/simple-uart.h>
   #include <sys/printf.h>

   static void putc(void *p, char c) {
       uart_send(c);
   }

   int kernel_start() {
       init_printf(0, putc);
       printf("Hello from my example!\n");
       
       while (1) {
           // Main loop
       }
       
       return 0;
   }
   ```

4. Update `config.mk`:
   ```makefile
   KERNEL = examples/my-example
   BOOTLOADER = bootloaders/linked
   ```

5. Build and run:
   ```bash
   make
   make run-vnc
   ```

## Module Selection Guide

Common module combinations for different needs:

### Minimal Output (UART only)
```makefile
KERNEL_MODULES := drivers/simple-uart sys/printf
```

### With Interrupts and Timer
```makefile
KERNEL_MODULES := arm/irq drivers/timer drivers/simple-uart sys/printf
```

### Full-Featured (libc, multiplexing)
```makefile
KERNEL_MODULES := arm/mmu-basic libc/libc libc/libgcc arm/irq \
                  drivers/timer sys/mux drivers/mbox drivers/uart
```

### Graphics Output
```makefile
KERNEL_MODULES := arm/irq drivers/mbox drivers/display
```

## Viewing Output

### Serial Output
Connect to TCP port 4444:
```bash
nc localhost 4444
```

Or use the preconfigured terminal tabs in VS Code.

### VNC Display
For examples with graphics (`display`), connect a VNC client to `localhost:5901`, or use VS Code's Simple Browser.

## Further Reading

- [Build System](../docs/build-system.md) — How examples are built
- [Modules](../modules/README.md) — Available modules and their APIs
- [Bootloaders](../bootloaders/README.md) — Different bootloader options
