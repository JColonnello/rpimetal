#include <drivers/mbox.h>
#include <drivers/timer.h>
#include <stdbool.h>
#include <stddef.h>

#define MBOX_TAG_GET_LED 0x00030041
#define MBOX_TAG_SET_LED 0x00038041

static unsigned int led_pin;
static volatile bool toggle_pending = false;

// Adhoc definition of memcpy
void *memcpy(void *dest, const void *src, size_t n)
{
	char *place = (char *)dest;
	const char *handler = (const char *)src;
	for (size_t i = 0; i < n; i++)
		place[i] = handler[i];
	return dest;
}

static void on_tick(unsigned id, void *data)
{
	toggle_pending = true;
}

static int led_init(void)
{
	mbox[0] = 8 * 4;
	mbox[1] = MBOX_REQUEST;
	mbox[2] = MBOX_TAG_GET_LED;
	mbox[3] = 8;
	mbox[4] = 0;
	mbox[5] = 0;
	mbox[6] = 0;
	mbox[7] = MBOX_TAG_LAST;
	mbox_call(MBOX_CH_PROP);
	mbox_wait();

	if (!(mbox[1] & 0x80000000))
		return -1;

	led_pin = mbox[5];
	return 0;
}

static int led_set(int on)
{
	mbox[0] = 8 * 4;
	mbox[1] = MBOX_REQUEST;
	mbox[2] = MBOX_TAG_SET_LED;
	mbox[3] = 8;
	mbox[4] = 0;
	mbox[5] = led_pin;
	mbox[6] = on;
	mbox[7] = MBOX_TAG_LAST;
	mbox_call(MBOX_CH_PROP);
	mbox_wait();

	return mbox[6];
}

int kernel_start()
{
	if (led_init() < 0)
		return -1;

	int state = 0;
	timer_register(1000000, true, on_tick, NULL);

	while (1)
	{
		if (toggle_pending)
		{
			toggle_pending = false;
			state = !state;
			led_set(state);
		}
		asm("wfi");
	}

	return 0;
}
