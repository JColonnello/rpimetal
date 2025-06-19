#pragma once

void timer_init(void);
void timer_handler(void);
void timer_microsleep(unsigned int micros);
void timer_millisleep(unsigned int millis);
unsigned timer_register(unsigned long micros, void (*handler)(unsigned, void *), void *);
void timer_unregister(unsigned id);
void timer_count(unsigned long *count1, unsigned long *count2);
