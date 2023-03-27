#include "utils.h"
#include "printf.h"
#include "peripherals/timer.h"

const unsigned int interval = 20000000;
unsigned int curVal = 0;

void timer_init ( void )
{
	// Set value, enable Timer and Interrupt
	put32(TIMER_CTRL, ((1<<28) | (1<<29) | interval));
}

void timer_reload()
{
	// Clear interrupt and reload timer
	put32(TIMER_FLAG, (1<<31));
}

void handle_timer_irq( void ) 
{
	printf("Timer interrupt received, Local timer\n\r");
	timer_reload();
}
