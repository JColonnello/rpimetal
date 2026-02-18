/*
 * Copyright (C) 2018 bzt (bztsrc@github)
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 */

#include "arm/irq.h"
#include <attrib.h>
#include <drivers/gpio.h>
#include <drivers/mbox.h>
#include <drivers/uart.h>
#include <ringbuffer.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* PL011 UART registers */
#define UART0_DR ((volatile uint32_t *)(MMIO_BASE + 0x00201000))
#define UART0_FR ((volatile uint32_t *)(MMIO_BASE + 0x00201018))
#define UART0_IBRD ((volatile uint32_t *)(MMIO_BASE + 0x00201024))
#define UART0_FBRD ((volatile uint32_t *)(MMIO_BASE + 0x00201028))
#define UART0_LCRH ((volatile uint32_t *)(MMIO_BASE + 0x0020102C))
#define UART0_CR ((volatile uint32_t *)(MMIO_BASE + 0x00201030))
#define UART0_IFLS ((volatile uint32_t *)(MMIO_BASE + 0x00201034))
#define UART0_IMSC ((volatile uint32_t *)(MMIO_BASE + 0x00201038))
#define UART0_RIS ((volatile uint32_t *)(MMIO_BASE + 0x0020103C))
#define UART0_MIS ((volatile uint32_t *)(MMIO_BASE + 0x00201040))
#define UART0_ICR ((volatile uint32_t *)(MMIO_BASE + 0x00201044))
#define UART0_ITCR ((volatile uint32_t *)(MMIO_BASE + 0x00201080))
#define UART0_ITIP ((volatile uint32_t *)(MMIO_BASE + 0x00201084))
#define UART0_ITOP ((volatile uint32_t *)(MMIO_BASE + 0x00201088))
#define UART0_TDR ((volatile uint32_t *)(MMIO_BASE + 0x0020108C))

// Definitions from Raspberry PI Remote Serial Protocol.
//     Copyright 2012 Jamie Iles, jamie@jamieiles.com.
//     Licensed under GPLv2
#define DR_OE_MASK (1 << 11)
#define DR_BE_MASK (1 << 10)
#define DR_PE_MASK (1 << 9)
#define DR_FE_MASK (1 << 8)

#define FR_TXFE_MASK (1 << 7)
#define FR_RXFF_MASK (1 << 6)
#define FR_TXFF_MASK (1 << 5)
#define FR_RXFE_MASK (1 << 4)
#define FR_BUSY_MASK (1 << 3)

#define LCRH_SPS_MASK (1 << 7)
#define LCRH_WLEN8_MASK (3 << 5)
#define LCRH_WLEN7_MASK (2 << 5)
#define LCRH_WLEN6_MASK (1 << 5)
#define LCRH_WLEN5_MASK (0 << 5)
#define LCRH_FEN_MASK (1 << 4)
#define LCRH_STP2_MASK (1 << 3)
#define LCRH_EPS_MASK (1 << 2)
#define LCRH_PEN_MASK (1 << 1)
#define LCRH_BRK_MASK (1 << 0)

#define CR_CTSEN_MASK (1 << 15)
#define CR_RTSEN_MASK (1 << 14)
#define CR_OUT2_MASK (1 << 13)
#define CR_OUT1_MASK (1 << 12)
#define CR_RTS_MASK (1 << 11)
#define CR_DTR_MASK (1 << 10)
#define CR_RXE_MASK (1 << 9)
#define CR_TXE_MASK (1 << 8)
#define CR_LBE_MASK (1 << 7)
#define CR_EN_MASK (1 << 0)

#define IFLS_RXIFSEL_SHIFT 3
#define IFLS_RXIFSEL_MASK (7 << IFLS_RXIFSEL_SHIFT)
#define IFLS_TXIFSEL_SHIFT 0
#define IFLS_TXIFSEL_MASK (7 << IFLS_TXIFSEL_SHIFT)
#define IFLS_IFSEL_1_8 0
#define IFLS_IFSEL_1_4 1
#define IFLS_IFSEL_1_2 2
#define IFLS_IFSEL_3_4 3
#define IFLS_IFSEL_7_8 4

#define INT_OE (1 << 10)
#define INT_BE (1 << 9)
#define INT_PE (1 << 8)
#define INT_FE (1 << 7)
#define INT_RT (1 << 6)
#define INT_TX (1 << 5)
#define INT_RX (1 << 4)
#define INT_DSRM (1 << 3)
#define INT_DCDM (1 << 2)
#define INT_CTSM (1 << 1)

#define UART_STEP 12

uint32_t nLCRH = LCRH_FEN_MASK;
static char raw_rx_buffer[2048], raw_tx_buffer[2048];
static ring_buffer uart_rx_buffer, uart_tx_buffer;

static void _nothing(size_t _)
{
}
static void (*uart_rx_callback)(size_t available) = _nothing;
static void (*uart_tx_callback)(size_t available) = _nothing;

static void set_clock(unsigned long freq)
{
	mbox[0] = 9 * 4;
	mbox[1] = MBOX_REQUEST;
	mbox[2] = MBOX_TAG_SETCLKRATE; // set clock rate
	mbox[3] = 12;
	mbox[4] = 0;    // Request
	mbox[5] = 2;    // UART clock
	mbox[6] = freq; // 4Mhz
	mbox[7] = 1;    // clear turbo
	mbox[8] = MBOX_TAG_LAST;
	mbox_call(MBOX_CH_PROP);
	mbox_wait();
}

static unsigned long get_clock()
{
	mbox[0] = 8 * 4;
	mbox[1] = MBOX_REQUEST;
	mbox[2] = 0x30002; // get clock rate
	mbox[3] = 8;       // Data length
	mbox[4] = 0;       // Request
	mbox[5] = 2;       // UART clock
	mbox[6] = 0;       // Result
	mbox[7] = MBOX_TAG_LAST;
	mbox_call(MBOX_CH_PROP);
	mbox_wait();
	return mbox[6];
}

static unsigned long get_measured_clock()
{
	mbox[0] = 8 * 4;
	mbox[1] = MBOX_REQUEST;
	mbox[2] = 0x30047; // get clock rate measured
	mbox[3] = 8;       // Data length
	mbox[4] = 0;       // Request
	mbox[5] = 2;       // UART clock
	mbox[6] = 0;       // Result
	mbox[7] = MBOX_TAG_LAST;
	mbox_call(MBOX_CH_PROP);
	mbox_wait();
	return mbox[6];
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

static void handle_uart0(void *data)
{
	uint32_t flag = *UART0_FR, is = *UART0_MIS;
	unsigned i, j;
	bool has_data = true;

	// If there is nothing to read, skip
	if (!(is & INT_RX))
		goto tx;

	for (;;)
	{
		i = ring_buffer_capacity(&uart_rx_buffer);
		// If there is no more space in the buffer, me mask the interrupt until there is space
		if (i < UART_STEP)
		{
			*UART0_IMSC &= ~INT_RX; // disable RX interrupt
			break;
		}
		static char read_buffer[UART_STEP];
		for (j = 0; j < UART_STEP; i--, j++)
		{
			static uint32_t last_fr;
			while ((flag = *UART0_FR) & FR_RXFE_MASK)
			{
				last_fr++;
				has_data = false;
				if (last_fr > 100)
				{
					fprintf(stderr, "UART RX missing bytes. Waiting\n");
					break;
				}
			}
			char c = (char)(*UART0_DR);
			read_buffer[j] = c;
		}
		ring_buffer_queue_arr(&uart_rx_buffer, read_buffer, UART_STEP);
		is = *UART0_MIS;
		if (!(is & INT_RX))
			has_data = false;

		// Signal the callback
		uart_rx_callback(sizeof(raw_rx_buffer) - i);
		// If there is nothing else to read, stop
		if (!has_data)
			break;
	}

tx:
	if (!(is & INT_TX))
		return;
	// If there is no space to send, skip
	if (flag & FR_TXFF_MASK)
		return;

	i = ring_buffer_num_items(&uart_tx_buffer);
	j = 0;
	do
	{
		for (; i > 0; j++)
		{
			char c = ring_buffer_dequeue_nc(&uart_tx_buffer);
			*UART0_DR = c;
			flag = *UART0_FR;
			i--;

			if (flag & FR_TXFF_MASK || j >= 16)
				return;
		}

		// There is space available, signal the callback
		uart_tx_callback(sizeof(raw_tx_buffer) - i);
		i = ring_buffer_num_items(&uart_tx_buffer);

		// If there is no more data in the buffer, me mask the interrupt until there is data
	} while (i > 0);
	*UART0_IMSC &= ~INT_TX; // disable TX interrupt
}

void uart_plain_mode()
{
	int16_t buf[6] = {INT16_MIN, 0};
	uart_send_buffer((char *)buf, sizeof(buf));
}

weak unsigned uart_target_baud = 921600; // Desired baud rate
weak enum uart_mode uart_mode = UART_MODE_PLAIN;

/**
 * Set baud rate and characteristics (115200 8N1) and map to GPIO
 */
constructor static void uart_init()
{
	const unsigned long target_freq = 48000000; // Desired UART clock frequency

	// Initialize ring buffers
	ring_buffer_init(&uart_rx_buffer, raw_rx_buffer, sizeof(raw_rx_buffer));
	ring_buffer_init(&uart_tx_buffer, raw_tx_buffer, sizeof(raw_tx_buffer));

	if (*UART0_CR)
	{
		*UART0_CR &= ~CR_RXE_MASK;       // Disable reception
		*UART0_LCRH &= ~LCRH_FEN_MASK;   // Disable FIFOs;
		while (*UART0_FR & FR_BUSY_MASK) // Wait for UART to be idle
			asm volatile("nop");
		*UART0_CR = 0; // Turn off UART0
	}
	if (uart_mode == UART_MODE_PLAIN)
		uart_plain_mode();

	map_pins();
	// *UART0_FBRD = 4;
	// *UART0_IBRD = 13;
	unsigned idiv, fdiv, baud;
	unsigned long freq;
	float divisor;

	fdiv = *UART0_FBRD;
	idiv = *UART0_IBRD;
	freq = get_clock(); // This enables interrupts too
	divisor = fdiv / 64.0f + idiv;
	baud = divisor != 0. ? (unsigned)(freq / (16 * divisor)) : 0;
	fprintf(
		stderr, "Current UART clock: %lu, divisor = %u + %u/64 = %.3f, baud rate: %u\n", freq, idiv, fdiv, divisor, baud
	);

	if (freq != target_freq)
		set_clock(target_freq);
	freq = get_measured_clock();
	divisor = freq / (16.0f * uart_target_baud);
	idiv = (unsigned)divisor;
	fdiv = (unsigned)((divisor - (unsigned)divisor) * 64 + 0.5f);
	divisor = fdiv / 64.0f + idiv;
	baud = divisor != 0. ? (unsigned)(freq / (16 * divisor)) : 0;
	fprintf(
		stderr,
		"New UART clock: %lu, divisor = %u + %u/64 = %.3f, target baud rate: %u, true baud rate: %u, error: %.2f%%\n",
		freq,
		idiv,
		fdiv,
		divisor,
		uart_target_baud,
		baud,
		((float)baud - uart_target_baud) / uart_target_baud * 100.0f
	);

	/* initialize UART */
	*UART0_ICR = 0x7FF; // clear interrupts
	*UART0_IBRD = idiv;
	*UART0_FBRD = fdiv;
	*UART0_LCRH = 0x7 << 4; // 8n1, enable FIFOs
	*UART0_IFLS = IFLS_IFSEL_1_8 << IFLS_TXIFSEL_SHIFT | IFLS_IFSEL_3_4 << IFLS_RXIFSEL_SHIFT;
	*UART0_IMSC = 0;
	irq_register(handle_uart0, NULL, GPU_INTERRUPT2, 57);

	// The TX interrupt does not get signaled until sending something
	// We send dummy characters and enable interrupts
	*UART0_CR = CR_EN_MASK | CR_TXE_MASK | CR_RXE_MASK;
	for (int i = UART_STEP; i--;)
		*UART0_DR = 0;
	*UART0_IMSC = INT_RX | INT_TX;
	while (*UART0_FR & FR_BUSY_MASK)
		asm volatile("nop");
}

static destructor void uart_destructor()
{
	*UART0_CR &= ~CR_RXE_MASK; // Turn off RX
	uart_flush_tx();
	*UART0_IMSC = 0;
	while (*UART0_FR & FR_BUSY_MASK)
		asm volatile("nop");
	*UART0_CR = 0; // Turn off
	irq_unregister(GPU_INTERRUPT2, 57);
}

static inline void signal_tx()
{
	*UART0_IMSC |= INT_TX;
}

static inline void signal_rx()
{
	*UART0_IMSC |= INT_RX;
}

void uart_send_raw(const char *s, size_t n)
{
	for (int i = 0; i < n; i++)
	{
		while (*UART0_FR & FR_TXFF_MASK)
			asm volatile("nop");
		*UART0_DR = s[i];
	}
}

/**
 * Send a character
 */
bool uart_send(char c)
{
	if (!ring_buffer_queue(&uart_tx_buffer, c))
		return false;
	signal_tx();
	return true;
}

/**
 * Receive a character
 */
bool uart_recv(char *c)
{
	bool r = ring_buffer_dequeue(&uart_rx_buffer, c);
	signal_rx();
	return r;
}

/**
 * Display a string
 */
size_t uart_send_string(const char *s)
{
	size_t count = 0;
	while (*s)
	{
		if (ring_buffer_queue(&uart_tx_buffer, *s++))
			count++;
	}
	signal_tx();
	return count;
}

/**
 * Display a buffer of chars
*/
size_t uart_send_buffer(const char *s, size_t n)
{
	size_t count = ring_buffer_queue_arr(&uart_tx_buffer, s, n);
	signal_tx();
	return count;
}

size_t uart_recv_buffer(char *s, size_t n)
{
	size_t count = ring_buffer_dequeue_arr(&uart_rx_buffer, s, n);
	signal_rx();
	return count;
}

/**
 * Display a binary value in hexadecimal
 */
void uart_hex(unsigned int d)
{
	unsigned int n;
	int c;
	for (c = 28; c >= 0; c -= 4)
	{
		// get highest tetrad
		n = (d >> c) & 0xF;
		// 0-9 => '0'-'9', 10-15 => 'A'-'F'
		n += n > 9 ? 0x37 : 0x30;
		uart_send(n);
	}
}

void uart_set_rx_callback(void (*handler)(size_t available))
{
	uart_rx_callback = handler;
}

void uart_set_tx_callback(void (*handler)(size_t available))
{
	uart_tx_callback = handler;
}

size_t uart_rx_available()
{
	return ring_buffer_num_items(&uart_rx_buffer);
}

size_t uart_tx_available()
{
	return ring_buffer_capacity(&uart_tx_buffer);
}

void uart_flush_tx()
{
	while (ring_buffer_num_items(&uart_tx_buffer) > 0)
		asm("wfi");
}