#pragma once
#include <attrib.h>
#include <stdbool.h>
#include <sys/_intsup.h>

void timer_fini(void);
void timer_microsleep(unsigned long micros);
void timer_millisleep(unsigned long millis);
int timer_register(unsigned long micros, bool repeating, void (*handler)(unsigned, void *), void *data);
bool timer_unregister(unsigned id);

// Changes the data associated with a timer
// Returns true if the timer was found, false if not
// If the timer is found, *old_data will be set to the previous data
// If new_data is NULL, timer data is not changed
// If new_data is not NULL, the timer data is set to *new_data
bool timer_set_data(unsigned id, void **old_data, void *const *new_data);

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