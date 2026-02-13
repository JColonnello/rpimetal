#pragma once
#include <stdbool.h>
#include <stddef.h>

bool uart_recv(char *c);
bool uart_send(char c);
size_t uart_send_string(const char *str);
size_t uart_send_buffer(const char *s, size_t n);
void uart_set_rx_callback(void (*handler)(size_t available));
void uart_set_tx_callback(void (*handler)(size_t available));
size_t uart_recv_buffer(char *s, size_t n);
size_t uart_rx_available();
size_t uart_tx_available();
void uart_send_raw(const char *s, size_t n);
void uart_flush_tx();

extern unsigned uart_target_baud; // Desired baud rate
enum uart_mode
{
	UART_MODE_PLAIN,
	UART_MODE_MUXED,
};
extern enum uart_mode uart_mode;
