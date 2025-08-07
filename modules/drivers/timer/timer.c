#define NO_TIMER_WEAK
#include "timer.h"
#include "arm/irq.h"
#include "attrib.h"
#include "utils.h"
#include <assert.h>
#include <drivers/irq.h>
#include <drivers/timer.h>
#include <limits.h>
#include <sys/_intsup.h>
#include <sys/stat.h>

#define LOCAL_TIMER_INTERVAL 38400 // 1ms
_Static_assert(LOCAL_TIMER_INTERVAL < (1 << 28), "Interval must be less than 2^28");
static unsigned long freq;

extern void stop_generic_timer(void);
extern void timer_handler(void);

constructor static void timer_init(void)
{
	irq_fiq_handler(stop_generic_timer);
	// Redirect generic timer interrupt to FIQ
	mreg32(CORE0_INT_CTR) = 1 << 5;
	// Redirect locar timer interrupt to Core 0 IRQ
	mreg32(TIMER_LIR) = 0b000;
	irq_register((void (*)(void *))timer_handler, NULL, LOCAL_INTERRUPT, 11);
	// Set value, enable local timer and Interrupt
	mreg32(TIMER_CTRL) = ((1 << 28) | (1 << 29) | LOCAL_TIMER_INTERVAL);
	// Get the frequency of the system counter
	asm("mrs %0, CNTFRQ_EL0" : "=r"(freq));
}

destructor void timer_fini(void)
{
	// Disable local timer
	mreg32(TIMER_CTRL) = 0;
	// Unregister the timer handler
	irq_unregister(LOCAL_INTERRUPT, 11);
	// Disable generic timer interrupt
	mreg32(CORE0_INT_CTR) &= ~(1 << 5);
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

static void pack_timers()
{
	// Pack the timers array to remove gaps
	unsigned i, j;
	for (i = 0, j = 0; i < MAX_TIMERS; i++)
	{
		if (timers[i].active)
		{
			if (i != j)
			{
				timers[j] = timers[i]; // Move active timer to the front
			}
			j++;
		}
	}
	for (; j < MAX_TIMERS; j++)
	{
		timers[j].active = false; // Mark remaining as inactive
	}
}

void timer_handler()
{
	// Clear interrupt of local timer
	mreg32(TIMER_FLAG) = (1 << 31);
	unsigned long system_counter;
	int i;

	// Get current system counter
	asm("mrs %0, CNTPCT_EL0" : "=r"(system_counter));

	// Iterate over all timers until we found one inactive
	// or we reach the end of the list
	// Call handler if next_tick > local_counter
	// If periodic, add interval to next_tick
	bool pack = false;
	for (i = 0; i < MAX_TIMERS; i++)
	{
		if (!timers[i].active)
			break;
		if (timers[i].next_tick <= system_counter)
		{
			timers[i].handler(timers[i].id, timers[i].param);
			if (timers[i].periodic)
				timers[i].next_tick += timers[i].interval;
			else
			{
				pack = true;              // Mark for packing
				timers[i].active = false; // deactivate timer
			}
		}
	}
	// Pack timers if needed
	if (pack)
		pack_timers();
}

void timer_microsleep(unsigned long micros)
{
	assert(micros < INT64_MAX); // Ensure micros is within bounds
	// Calculate target tick
	uint64_t target, system;
	asm("mrs %0, CNTPCT_EL0" : "=r"(system));
	target = system + micros * freq / 1000000;

	asm("msr CNTP_CVAL_EL0, %0\n"
		"msr CNTP_CTL_EL0, %1\n"
		:
		: "r"(target), "r"(1l));
	do
	{
		// Wait for the timer to expire
		asm("wfi\n"
			"mrs %0, CNTPCT_EL0\n"
			: "=r"(system));
	} while ((int64_t)(target - system) > 0);
}

void timer_millisleep(unsigned long millis)
{
	timer_microsleep(millis * 1000);
}

int timer_register(unsigned long micros, bool repeating, void (*handler)(unsigned, void *), void *data)
{
	unsigned i;
	// Search first free timer slot
	for (i = 0; i < MAX_TIMERS; i++)
	{
		if (!timers[i].active)
			break;
	}
	if (i == MAX_TIMERS)
		return -1; // No free slots

	unsigned long system_counter;
	asm("mrs %0, CNTPCT_EL0" : "=r"(system_counter));

	// Initialize timer
	unsigned long interval = micros * freq / 1000000; // Convert micros to system counter ticks
	timers[i] = (Timer){
		.active = true,
		.periodic = repeating,
		.id = handler_id_curr++,
		.interval = interval,
		.next_tick = system_counter + interval,
		.handler = handler,
		.param = data,
	};

	return i;
}

bool timer_unregister(unsigned id)
{
	if (id >= handler_id_curr)
		return false; // Invalid ID

	for (unsigned i = 0; i < MAX_TIMERS && timers[i].active; i++)
	{
		if (timers[i].id == id)
		{
			timers[i].active = false; // Deactivate timer
			pack_timers();            // Pack timers to remove gaps
			return true;              // Successfully unregistered
		}
	}
	return false; // Timer not found
}

unsigned long timer_monotonic(void)
{
	unsigned long system_counter;
	asm("mrs %0, CNTPCT_EL0" : "=r"(system_counter));
	return system_counter * 1000000 / freq; // Convert to microseconds
}

bool timer_set_data(unsigned id, void **old_data, void *const *new_data)
{
	if (id >= handler_id_curr)
		return false; // Invalid ID

	for (unsigned i = 0; i < MAX_TIMERS && timers[i].active; i++)
	{
		if (timers[i].id == id)
		{
			if (old_data)
				*old_data = timers[i].param; // Store old data if requested
			if (new_data)
				timers[i].param = *new_data; // Update to new data
			return true;                     // Successfully updated
		}
	}
	return false; // Timer not found
}
