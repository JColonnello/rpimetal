#pragma once

void uart_send(char c);
void uart_set_rx_callback(void (*handler)(char c));
