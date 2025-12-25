#include <arm/irq.h>
#include <attrib.h>
#include <drivers/gpio.h>
#include <drivers/simple-uart.h>
#include <stddef.h>
#include <stdint.h>

/* PL011 UART registers */
#define UART0_DR ((volatile uint32_t *)(MMIO_BASE + 0x00201000))
#define UART0_FR ((volatile uint32_t *)(MMIO_BASE + 0x00201018))
#define UART0_IBRD ((volatile uint32_t *)(MMIO_BASE + 0x00201024))
#define UART0_FBRD ((volatile uint32_t *)(MMIO_BASE + 0x00201028))
#define UART0_LCRH ((volatile uint32_t *)(MMIO_BASE + 0x0020102C))
#define UART0_CR ((volatile uint32_t *)(MMIO_BASE + 0x00201030))
#define UART0_IMSC ((volatile uint32_t *)(MMIO_BASE + 0x00201038))
#define UART0_ICR ((volatile uint32_t *)(MMIO_BASE + 0x00201044))

#define FR_TXFF_MASK (1 << 5)
#define FR_BUSY_MASK (1 << 3)

#define LCRH_FEN_MASK (1 << 4)

#define CR_RXE_MASK (1 << 9)
#define CR_TXE_MASK (1 << 8)
#define CR_EN_MASK (1 << 0)

#define INT_RX (1 << 4)

static void (*uart_rx_callback)(char c);
void uart_set_rx_callback(void (*handler)(char c))
{
	uart_rx_callback = handler;
}

/**
 * Send a character
 */
void uart_send(char c)
{
	/* wait until we can send */
	while (*UART0_FR & FR_TXFF_MASK)
		asm volatile("nop");
	/* write the character to the buffer */
	*UART0_DR = c;
}

/**
 * Handle UART RX interrupt
 */
static void handle_uart0(void *data)
{
	char c = *UART0_DR;
	if (uart_rx_callback)
		uart_rx_callback(c);
}

static void map_pins()
{
	unsigned int r;

	/* map UART0 to GPIO pins */
	r = *GPFSEL1;
	r &= ~((7 << 12) | (7 << 15)); // gpio14, gpio15
	r |= (4 << 12) | (4 << 15);    // alt0
	*GPFSEL1 = r;
	*GPPUD = 0; // enable pins 14 and 15
	for (int i = 150; i--;)
		asm volatile("nop");
	*GPPUDCLK0 = (1 << 14) | (1 << 15);
	for (int i = 150; i--;)
		asm volatile("nop");
	*GPPUDCLK0 = 0; // flush GPIO setup
}

/**
 * Set baud rate and characteristics (115200 8N1) and map to GPIO
 */
constructor static void uart_init()
{
	if (*UART0_CR)
	{
		*UART0_CR &= ~CR_RXE_MASK;       // Disable reception
		*UART0_LCRH &= ~LCRH_FEN_MASK;   // Disable FIFOs;
		while (*UART0_FR & FR_BUSY_MASK) // Wait for UART to be idle
			asm volatile("nop");
		*UART0_CR = 0; // Turn off UART0
	}
	map_pins();

	/* initialize UART */
	*UART0_ICR = 0x7FF; // clear interrupts
	*UART0_IBRD = 2;
	*UART0_FBRD = 0xB;
	*UART0_LCRH = 0b11 << 5; // 8n1
	*UART0_IMSC = 0;
	irq_register(handle_uart0, NULL, GPU_INTERRUPT2, 57);
	*UART0_CR = CR_EN_MASK | CR_TXE_MASK | CR_RXE_MASK;
	*UART0_IMSC = INT_RX;
}

static destructor void uart_destructor()
{
	*UART0_CR = 0; // Turn off
	*UART0_IMSC = 0;
	irq_unregister(GPU_INTERRUPT2, 57);
}
