#ifndef _IRQ_H
#define _IRQ_H

void enable_interrupt_controller(void);

void irq_vector_init(void);
void enable_irq(void);
void disable_irq(void);
void register_fiq(void (*handler)(void));

#endif /*_IRQ_H */
