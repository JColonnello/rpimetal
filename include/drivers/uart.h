#ifndef _UART_H
#define _UART_H
#include <stddef.h>

char uart_recv(void);
void uart_send(char c);
void uart_send_string(const char *str);
void uart_hex(unsigned int d);
void uart_send_buffer(const char *s, size_t n);

#endif /*_UART_H */
