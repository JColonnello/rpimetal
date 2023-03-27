#ifndef	_UART_H
#define	_UART_H

void uart_init ( void );
char uart_recv ( void );
void uart_send ( char c );
void uart_send_string(char* str);
void putc ( void* p, char c );
void uart_hex(unsigned int d);
void handle_uart0( void );

#endif  /*_UART_H */
