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

After that, every character you type is echoed back as its hex value (typing `a` prints `61`).

## Files

### Makefile

```makefile
KERNEL_MODULES := arm/irq drivers/simple-uart sys/printf
```

This example uses minimal modules:
- `arm/irq`: Interrupt handling (simple-uart registers its RX interrupt here)
- `drivers/simple-uart`: Basic UART driver for serial input and output
- `sys/printf`: Lightweight printf implementation

### kernel.c

Let's examine the code section by section:

```c
#include <drivers/simple-uart.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/printf.h>
```

Include the necessary headers:
- `drivers/simple-uart.h`: Provides `uart_send()` and `uart_set_rx_callback()`
- `sys/printf.h`: Provides `init_printf()` and `printf()`

```c
static void _putc(void *p, char c)
{
    uart_send(c);
}
```

The `printf` module needs a function to output individual characters. We define `_putc` as a wrapper around `uart_send()`. The `void *p` parameter is unused but required by the interface.


```c
static void uart_receive_callback(char c)
{
    // Echo received character
    printf("%x", c);
}
```

Called from the UART RX interrupt for each received character. It prints the character's value in hex.

```c
int kernel_start()
{
    init_printf(0, _putc);
    printf("Hello, RPi Metal!\n");
    uart_set_rx_callback(uart_receive_callback);
    for (;;)
        asm("wfi");

    return 0;
}
```

The main entry point:
1. `init_printf(0, _putc)`: Initialize printf with our output function
2. `printf(...)`: Print our message
3. `uart_set_rx_callback(...)`: Register the echo callback
4. `for (;;) asm("wfi")`: Sleep until the next interrupt, forever. `kernel_start` never returns, so the `return 0` is unreachable.

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

3. Start a serial listener (QEMU connects to it as a client, so it must be running first):
   ```bash
   make uart0     # nc -lkvp 4444
   ```

4. In another terminal, run:
   ```bash
   make run
   ```

You should see "Hello, RPi Metal!" printed in the listener terminal. Type there to see the hex echo.

## Exercises

1. **Change the message**: Modify the string and rebuild. Notice how fast the edit-compile-run cycle is.

2. **Multiple lines**: Add more `printf()` calls with different messages.

3. **Format specifiers**: Try using printf format specifiers:
   ```c
   int x = 42;
   printf("The answer is %d\n", x);
   ```

4. **Print periodically**: Replace the `wfi` loop with one that prints periodically:
   ```c
   while (1) {
       printf("Still running...\n");
       for (volatile int i = 0; i < 10000000; i++);  // Simple delay
   }
   ```

5. **Add a module**: Try adding `drivers/timer` to KERNEL_MODULES and use `timer_monotonic()` for better timing.

## Common Issues

### No output appears

- Make sure the listener (`make uart0` or `nc -lkvp 4444`) is running before `make run`
- Check that QEMU started successfully
- Verify the bootloader is `bootloaders/linked`

### Compilation errors

- Make sure all required modules are listed in KERNEL_MODULES
- Check include paths if headers aren't found

## Next Steps

- Try the [display](../display/README.md) example to see graphics output
- Try the [0A_misc](../0A_misc/README.md) example for a more complex program with input handling
