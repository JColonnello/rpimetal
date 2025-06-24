#pragma once
#include <attrib.h>
#include <stdbool.h>
#include <sys/_intsup.h>

void timer_microsleep(unsigned long micros);
void timer_millisleep(unsigned long millis);
int timer_register(unsigned long micros, bool repeating, void (*handler)(unsigned, void *), void *data);
bool timer_unregister(unsigned id);

// Gets the current system time in microseconds
unsigned long timer_monotonic(void);

#ifndef NO_TIMER_WEAK
// Replacement for timer_microsleep
weak void timer_microsleep(unsigned long us)
{
	for (unsigned long i = 0; i < us * 500; i++)
		asm("nop");
}

weak void timer_millisleep(unsigned long millis)
{
	timer_microsleep(millis * 1000);
}

#endif