#include <stdint.h>
#include <errno.h>
#include <peripherals/uart.h>
#include <stdio.h>
#include <sys/stat.h>
#include <stdbool.h>

#define stub(func) void func(void) { \
    undef_func(#func); \
    proc_hang(); \
}

extern const void __bss_end__;
static void* curr_break = (void*)&__bss_end__;
__attribute__((noreturn)) extern void proc_hang();

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

int _write(int fd, const void *buf, size_t count)
{
    uart_send_buffer(buf, count);
    return count;
}

void undef_func(const char *func)
{
    printf("Function undefined: %s\n", func);
    proc_hang();
}

int _fstat(int fildes, struct stat *buf)
{
    if(fildes >= 0 && fildes <= 2)
    {
        *buf = (struct stat)
        {
            .st_mode = S_IFCHR,
        };
        return 0;
    }
    errno = EBADF;
    return -1;
}

int _isatty(int fd)
{
    if(fd >= 0 && fd <= 2)
        return true;
    else
    {
        errno = EBADF;
        return false;
    }
}

int _close(int fd)
{
    if(fd >= 0 && fd <= 2)
        return 0;
    else
    {
        errno = EBADF;
        return -1;
    }
}

void _exit(int status)
{
    printf("Exitting kernel: %d\n", status);
    proc_hang();
}

stub(_lseek);
stub(_read);
stub(_kill);
stub(_getpid);
