# Tutorial 3: 0A_misc

An interactive program that echoes user input with timing information. This example demonstrates using the full C library, interrupt-driven I/O, timer functionality, and the serial multiplexer.

## What You'll Learn

- Using the full Newlib C library
- Reading from stdin (UART input)
- Using the timer for timing measurements
- Constructor/destructor functions
- Memory management with boot info
- Multiplexed serial communication

## Expected Output

When you run this example and type input:

```
(you type) Hello
Current time: 1234567 us
Received 5 bytes: Hello

(you type) World
Current time: 2345678 us
Received 5 bytes: World
```

If you exit (Ctrl+C or program termination):
```
Exitting kernel
```

## Files

### Makefile

```makefile
KERNEL_MODULES := arm/mmu-basic libc/libc libc/libgcc arm/irq \
                  drivers/timer sys/mux drivers/mbox drivers/uart
```

This example uses the full module set:
- `arm/mmu-basic`: Memory Management Unit setup
- `libc/libc`, `libc/libgcc`: Full C standard library
- `arm/irq`: Interrupt handling
- `drivers/timer`: Hardware timer
- `sys/mux`: Serial multiplexing
- `drivers/mbox`: Mailbox interface
- `drivers/uart`: Full UART driver (not simple-uart)

### stdlib.h / stdlib.c

Local helper for memory management and libc syscall adapters.

```c
// stdlib.h
void stdlib_set_mem_limits(void *start, void *end);

// stdlib.c
void stdlib_set_mem_limits(void *start, void *end) {
    heap_start = start;
    heap_end = end;
    current_brk = heap_start;
}
```

This file provides the minimal syscall stubs and memory-limit helpers
used by Newlib/libc (see `examples/0A_misc/stdlib.c` for the implemented
syscalls). Call `stdlib_set_mem_limits()` from `_init()` so `malloc()` and
other heap-using functions work correctly.

### foo.c

The main program file:

```c
#include "stdlib.h"
#include <attrib.h>
#include <boot/custom.h>
#include <drivers/timer.h>
#include <stdio.h>
#include <sys/unistd.h>
```

Headers used:
- `stdlib.h`: Local memory management helpers
- `attrib.h`: Macros for `constructor`, `destructor`, `weak`, etc.
- `boot/custom.h`: Boot info and custom data structures
- `drivers/timer.h`: Timer functions
- `stdio.h`: Standard I/O (from Newlib)
- `sys/unistd.h`: `read()` function

```c
destructor static void print_exit()
{
    printf("Exitting kernel\n");
}
```

Functions marked `destructor` are called when the program exits. This is useful for cleanup or final messages.

```c
void _init(struct boot_info *info, union boot_userdata userdata)
{
    stdlib_set_mem_limits(info->memory_start, info->memory_end);
}
```

The `_init` function is called early in startup. Here we configure the heap limits from boot info so `malloc()` and friends work correctly.

```c
int kernel_start(struct boot_info *info, union boot_userdata userdata)
{
    for (;;)
    {
        static char s[256];
        size_t n = read(0, s, sizeof(s));
        if (n)
        {
            unsigned long time = timer_monotonic();
            printf("Current time: %lu us\n", time);
            printf("Received %lu bytes: %.*s\n", n, (int)n, s);
        }
    }

    return 0;
}
```

The main loop:
1. **Read input**: `read(0, s, sizeof(s))` reads from stdin (channel 0)
2. **Check for data**: Only process if bytes were received
3. **Get time**: `timer_monotonic()` returns microseconds since boot
4. **Print output**: Display timing and received data
5. **Loop forever**: Continue reading input

## Key Concepts

### Boot Info

The kernel receives information about available memory:

```c
struct boot_info {
    void *boot_memory_end;  // End of bootloader memory
    void *memory_start;     // Start of heap
    void *memory_end;       // End of usable RAM (0x3E000000)
};
```

### Timing

The timer module provides:

```c
unsigned long timer_monotonic(void);  // Microseconds since boot
```

Useful for:
- Measuring elapsed time
- Implementing delays
- Profiling code

### Multiplexed I/O

With `sys/mux` and `drivers/uart`, standard I/O uses the multiplexing protocol:
- Channel 0: stdin/stdout
- Channel 1: stderr

This allows separate streams over a single serial connection.

### Constructor/Destructor

GCC attributes for automatic function calls:

```c
constructor static void my_init(void) {
    // Called before main
}

destructor static void my_cleanup(void) {
    // Called at exit
}
```

The `attrib.h` header provides convenient macros.

## Running the Example

1. Set up `config.mk`:
   ```makefile
   KERNEL = examples/0A_misc
   BOOTLOADER = bootloaders/linked
   ```

2. Build:
   ```bash
   make
   ```

3. For muxed mode (recommended):
   ```bash
   # Terminal 1: Start multiplexer
   make mux-tcp

   # Terminal 2: Run QEMU
   make run

   # Terminal 3: Connect to stdout
   nc localhost 4440
   ```

4. For plain mode (simpler but limited):
   ```bash
   # Terminal 1: Start listener
   nc -lkvp 4444

   # Terminal 2: Run QEMU
   make run
   ```

5. Type text in the connected terminal and press Enter

## Exercises

1. **Add timestamps**: Print time in a human-readable format (seconds.milliseconds)

2. **Command parser**: Instead of just echoing, parse simple commands:
   ```c
   if (strcmp(s, "time\n") == 0) {
       printf("Current time: %lu us\n", timer_monotonic());
   }
   ```

3. **Delay function**: Implement a delay using the timer:
   ```c
   void delay_ms(unsigned int ms) {
       unsigned long start = timer_monotonic();
       while (timer_monotonic() - start < ms * 1000);
   }
   ```

4. **Memory info**: Print available memory:
   ```c
   printf("Heap: %p - %p\n", info->memory_start, info->memory_end);
   ```

5. **Multiple channels**: Use mux to send different data to different channels:
   ```c
   mux_send(0, "stdout message\n", 15);
   mux_send(1, "stderr message\n", 15);
   ```

## Common Issues

### No input received

- Make sure you're connected to the right terminal
- For muxed mode, ensure the multiplexer is running
- Check that you press Enter after typing

### Garbage output

- Baud rate mismatch (should be 921600)
- Wrong terminal mode (plain vs muxed)

### Crashes on startup

- Memory limits not set correctly
- Missing modules in KERNEL_MODULES

## Technical Notes

### Why Two Files?

- `foo.c`: Main kernel code
- `stdlib.c`: Memory management bridge

The separation keeps memory management reusable across examples.

### Full UART vs Simple UART

`drivers/uart` provides:
- Buffered I/O
- Interrupt-driven receive/transmit
- Callbacks for data availability
- Integration with `sys/mux`

`drivers/simple-uart` provides:
- Lightweight UART support with interrupt-driven TX/RX (simpler API)
- Fewer dependencies and smaller footprint
- Does not provide multiplexing integration (use `drivers/uart` for mux)

### Memory Map

With this example:
```
0x00080000  ← Stack
            ← Kernel code and data
            ← memory_start (heap begins)
            ← malloc() allocations
            ...
0x3E000000  ← memory_end
```

## Next Steps

- Experiment with `drivers/sd` to read/write SD card
- Add `drivers/display` for combined text and graphics
- Explore the `loader` module for dynamic loading
