#pragma once
#include <stdbool.h>
#include <sys/_intsup.h>

void timer_init(void);
void timer_handler(void);
void timer_microsleep(unsigned long micros);
void timer_millisleep(unsigned long millis);
int timer_register(unsigned long micros, bool repeating, void (*handler)(unsigned, void *), void *data);
bool timer_unregister(unsigned id);

// Gets the current system time in microseconds
unsigned long timer_monotonic(void);
