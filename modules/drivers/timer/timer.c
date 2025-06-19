#include "timer.h"
#include "arm/irq.h"
#include "utils.h"
#include <stdbool.h>
#include <stdio.h>
#include <sys/_intsup.h>
#include <sys/stat.h>

static unsigned int interval = 1920000; // 192; // 5 us
unsigned long volatile counter1;
unsigned long volatile counter2;

void handle_timer_irq(void);

void timer_init(void)
{
	register_fiq(handle_timer_irq);
	// Redirect interrupt to FIQ
	mreg32(TIMER_LIR) = 0b100;
	// Set value, enable Timer and Interrupt
	interval &= (1 << 28) - 1;
	mreg32(TIMER_CTRL) = ((1 << 28) | (1 << 29) | interval);
}

__attribute__((unused)) void timer_reload()
{
	// Clear interrupt and reload timer
	mreg32(TIMER_FLAG) = (1 << 31);
}

#define MAX_TIMERS 16
typedef struct
{
	bool active;
	bool periodic;
	unsigned id;
	unsigned long interval;
	unsigned long next_tick;
	void (*handler)(unsigned, void *);
	void *param;
} Timer;
static Timer timers[MAX_TIMERS];
static unsigned handler_id_curr;

void timer_handler()
{
	unsigned long local_counter = counter1;
	unsigned i;
	// Iterate over all timers until we found one inactive
	// or we reach the end of the list
	// Call handler if next_tick > local_counter
	// If periodic, add interval to next_tick
	for (i = 0; i < MAX_TIMERS; i++)
	{
		if (!timers[i].active)
			break;
		if (timers[i].next_tick >= local_counter)
		{
			timers[i].handler(timers[i].id, timers[i].param);
			if (timers[i].periodic)
				timers[i].next_tick += timers[i].interval;
			else
				timers[i].active = false; // deactivate timer
		}
	}
	// Paste over the inactive timers other active timers from the tail of the list
	i--;
	for (unsigned j = 0; i >= 0 && j < i;)
	{
		if (!timers[i].active)
			i--;
		else if (!timers[j].active)
			timers[j++] = timers[i--];
		else
			j++;
	}
}

void timer_count(unsigned long *count1, unsigned long *count2)
{
	*count1 = counter1;
	*count2 = counter2;
}

void timer_microsleep(unsigned int micros);
void timer_millisleep(unsigned int millis);
unsigned timer_register(unsigned long micros, void (*handler)(unsigned, void *), void *);
void timer_unregister(unsigned id);