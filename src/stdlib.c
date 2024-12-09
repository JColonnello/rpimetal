#include <stdint.h>
#include <errno.h>
#include <peripherals/uart.h>
#include <stdio.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <unistd.h>

#define stub(func) void func(void) { \
    undef_func(#func); \
    proc_hang(); \
}

extern const void __end;
static void* curr_break = (void*)&__end;
__attribute__((noreturn)) extern void proc_hang();

void *_sbrk(intptr_t increment)
{
    if(increment == 0)
        return curr_break;
    if(curr_break + increment < &__end)
    {
        errno = ENOMEM;
        return (void*)-1;
    }
    
    void *last_break = curr_break;
    curr_break += increment;
    return last_break;
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
    // Unprintable
    // printf("Exitting kernel: %d\n", status);
    proc_hang();
}

long sysconf(int name)
{
    switch (name)
    {
        // The maximum number of files that a process can have open at any time.  Must not be less than _POSIX_OPEN_MAX (20)
        case _SC_OPEN_MAX:
            return 4096;
        case _SC_PAGESIZE:
            return 4096;
        default:
            printf("Unknown sysconf variable: %d\n", name);
            return 0;
    }
}
