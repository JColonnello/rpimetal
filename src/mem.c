#include <stdint.h>
#include <errno.h>

extern const void __bss_end__;
static void* curr_break = (void*)&__bss_end__;

void *_sbrk(intptr_t increment)
{
    if(increment == 0)
        return curr_break;
    if(curr_break + increment < &__bss_end__)
    {
        errno = ENOMEM;
        return (void*)-1;
    }
    
    curr_break += increment;
    return curr_break;
}
