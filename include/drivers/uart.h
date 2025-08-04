#pragma once
#include <stdbool.h>
#include <stddef.h>

bool uart_recv(char *c);
bool uart_send(char c);
size_t uart_send_string(const char *str);
size_t uart_send_buffer(const char *s, size_t n);
void uart_set_callback(void (*handler)(size_t available));
