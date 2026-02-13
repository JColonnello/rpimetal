#pragma once
#include <stddef.h>

void uart_send(char c);
void uart_set_rx_callback(void (*handler)(char c));
void uart_send_buffer(const char *buf, size_t len);
enum uart_mode
{
	UART_MODE_PLAIN,
	UART_MODE_MUXED,
};
extern enum uart_mode uart_mode;
