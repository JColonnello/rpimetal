# Tutorial 1: no-libc-uart

The simplest possible RPiMetal program — a "Hello, World!" that outputs text over the UART serial port.

## What You'll Learn

- Basic project structure
- Using the `simple-uart` driver
- Using the `printf` module
- Building and running with the `linked` bootloader

## Expected Output

When you run this example, you should see in the serial terminal:

```
Hello, RPi Metal!
```

## Files

### Makefile

```makefile
KERNEL_MODULES := arm/irq drivers/mbox drivers/simple-uart sys/printf
```

This example uses minimal modules:
- `arm/irq`: Interrupt handling (required by most drivers)
- `drivers/mbox`: Mailbox interface (required for UART clock setup)
- `drivers/simple-uart`: Basic UART driver for serial output
- `sys/printf`: Lightweight printf implementation

### kernel.c

Let's examine the code section by section:

```c
#include <drivers/simple-uart.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/printf.h>
```

Include the necessary headers:
- `drivers/simple-uart.h`: Provides `uart_send()` function
- `sys/printf.h`: Provides `init_printf()` and `printf()`

```c
static void _putc(void *p, char c)
{
    uart_send(c);
}
```

The `printf` module needs a function to output individual characters. We define `_putc` as a wrapper around `uart_send()`. The `void *p` parameter is unused but required by the interface.

```c
void *memcpy(void *dest, const void *src, size_t n)
{
    char *place = (char *)dest;
    const char *handler = (const char *)src;
    for (size_t i = 0; i < n; i++)
        place[i] = handler[i];
    return dest;
}
```

Since we're not using the full libc (`libc/libc` module), we need to provide `memcpy` ourselves. This is a simple byte-by-byte implementation. In more complex examples, we'd include `libc/libc` instead.

```c
int kernel_start()
{
    init_printf(0, _putc);
    printf("Hello, RPi Metal!\n");
    return 0;
}
```

The main entry point:
1. `init_printf(0, _putc)`: Initialize printf with our output function
2. `printf(...)`: Print our message
3. `return 0`: Exit (the bootloader halts the CPU)

## Running the Example

1. Set up `config.mk`:
   ```makefile
   KERNEL = examples/no-libc-uart
   BOOTLOADER = bootloaders/linked
   ```

2. Build:
   ```bash
   make
   ```

3. Run:
   ```bash
   make run
   ```

4. In another terminal, connect to the serial output:
   ```bash
   nc localhost 4444
   ```

You should see "Hello, RPi Metal!" printed.

## Exercises

1. **Change the message**: Modify the string and rebuild. Notice how fast the edit-compile-run cycle is.

2. **Multiple lines**: Add more `printf()` calls with different messages.

3. **Format specifiers**: Try using printf format specifiers:
   ```c
   int x = 42;
   printf("The answer is %d\n", x);
   ```

4. **Loop forever**: Instead of returning, add an infinite loop that prints periodically:
   ```c
   while (1) {
       printf("Still running...\n");
       for (volatile int i = 0; i < 10000000; i++);  // Simple delay
   }
   ```

5. **Add a module**: Try adding `drivers/timer` to KERNEL_MODULES and use `timer_monotonic()` for better timing.

## Common Issues

### No output appears

- Make sure `nc localhost 4444` is running before or shortly after `make run`
- Check that QEMU started successfully
- Verify the bootloader is `bootloaders/linked`

### Compilation errors

- Make sure all required modules are listed in KERNEL_MODULES
- Check include paths if headers aren't found

## Next Steps

- Try the [display](../display/) example to see graphics output
- Try the [0A_misc](../0A_misc/) example for a more complex program with input handling
