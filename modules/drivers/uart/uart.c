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
#include <stddef.h>
#include <stdint.h>

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
#define CR_UART_EN_MASK (1 << 0)

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

uint32_t nLCRH = LCRH_FEN_MASK;

static void handle_uart0(void *);

/**
 * Set baud rate and characteristics (115200 8N1) and map to GPIO
 */
constructor static void uart_init()
{
	register unsigned int r;

	/* initialize UART */
	*UART0_CR = 0; // turn off UART0

	/* set up clock for consistent divisor values */
	mbox[0] = 9 * 4;
	mbox[1] = MBOX_REQUEST;
	mbox[2] = MBOX_TAG_SETCLKRATE; // set clock rate
	mbox[3] = 12;
	mbox[4] = 8;
	mbox[5] = 2;       // UART clock
	mbox[6] = 4000000; // 4Mhz
	mbox[7] = 0;       // clear turbo
	mbox[8] = MBOX_TAG_LAST;
	mbox_call(MBOX_CH_PROP);

	/* map UART0 to GPIO pins */
	r = *GPFSEL1;
	r &= ~((7 << 12) | (7 << 15)); // gpio14, gpio15
	r |= (4 << 12) | (4 << 15);    // alt0
	*GPFSEL1 = r;
	*GPPUD = 0; // enable pins 14 and 15
	r = 150;
	while (r--)
	{
		asm volatile("nop");
	}
	*GPPUDCLK0 = (1 << 14) | (1 << 15);
	r = 150;
	while (r--)
	{
		asm volatile("nop");
	}
	*GPPUDCLK0 = 0; // flush GPIO setup

	*UART0_ICR = 0x7FF; // clear interrupts
	*UART0_IBRD = 2;    // 115200 baud
	*UART0_FBRD = 0xB;
	*UART0_LCRH = 0x7 << 4; // 8n1, enable FIFOs
	*UART0_IFLS = IFLS_IFSEL_1_8 << IFLS_TXIFSEL_SHIFT | IFLS_IFSEL_1_8 << IFLS_RXIFSEL_SHIFT;
	*UART0_LCRH = nLCRH;
	*UART0_IMSC = INT_RX | INT_RT | INT_OE;
	*UART0_CR = 0x301; // enable Tx, Rx, UART

	irq_register(handle_uart0, NULL, GPU_INTERRUPT2, 57);
}

uint16_t uart_ints()
{
	return *UART0_MIS;
}

/**
 * Send a character
 */
void uart_send(char c)
{
	/* wait until we can send */
	do
	{
		asm volatile("nop");
	} while (*UART0_FR & 0x20);
	/* write the character to the buffer */
	*UART0_DR = c;
}

static void handle_uart0(void *data)
{
	*UART0_ICR = 0;
	while (!(*UART0_FR & 0x10))
	{
		uart_send(uart_recv());
	}
	uart_send_string("Fin\n");
}

/**
 * Receive a character
 */
char uart_recv()
{
	char r;
	/* read it and return */
	r = (char)(*UART0_DR);
	return r;
}

/**
 * Display a string
 */
void uart_send_string(const char *s)
{
	while (*s)
	{
		uart_send(*s++);
	}
}

/**
 * Display a buffer of chars
*/
void uart_send_buffer(const char *s, size_t n)
{
	for (unsigned i = 0; i < n; i++)
		uart_send(s[i]);
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
