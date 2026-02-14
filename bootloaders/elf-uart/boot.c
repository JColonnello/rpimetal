#include "boot.h"
#include "complex-loader.h"
#include <arm/irq.h>
#include <attrib.h>
#include <boot/custom.h>
#include <drivers/irq.h>
#include <drivers/timer.h>
#include <drivers/uart.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mux.h>
#include <sys/unistd.h>

void fini()
{
}

static struct boot_customdata boot_data;
static struct boot_info boot_info;
static union boot_userdata boot_userdata;
static start_function_type kernel_start;
static struct tls_data *tcb;
static struct linkset *linkset;

void noreturn kernel_jump()
{
	loader_switch_tcb(tcb);
	asm("mov x0, %0\n"
		"ldr x1, %1\n"
		"ldr x2, %2\n"
		"add x30, x2, #4\n"
		"br x2\n"
		:
		: "r"(&boot_info), "m"(boot_userdata), "m"(kernel_start)
		: "x0", "x1", "x2", "x30");
	__builtin_unreachable();
}

/* Read a newline-terminated line from mux channel into buf (null-terminated). */
static ssize_t recv_line(int16_t channel, char *buf, size_t max)
{
	size_t idx = 0;
	char c;
	for (;;)
	{
		while (mux_rx_available(channel) == 0)
			;
		size_t n = mux_recv(channel, &c, 1);
		if (n == 0)
			continue;
		if (c == '\n')
		{
			if (idx < max)
				buf[idx] = '\0';
			else
				buf[max - 1] = '\0';
			return (ssize_t)idx;
		}
		if (idx + 1 < max)
			buf[idx++] = c;
	}
}

/* Read exactly `len` bytes from mux channel into buf. */
static int recv_n(int16_t channel, void *buf, size_t len)
{
	char *p = buf;
	size_t left = len;
	while (left > 0)
	{
		while (mux_rx_available(channel) == 0)
			;
		size_t n = mux_recv(channel, p, left);
		if (n == 0)
			continue;
		p += n;
		left -= n;
	}
	return 0;
}

int main(void)
{
	struct start_symbol symbols[] = {
		{.name = "__stack", .address = (void *)0x80000},
		{.name = "__bss_start__", .address = NULL},
		{.name = "__bss_end__", .address = NULL},
		{.name = "_fini", .address = fini},
	};

	linkset = loader_create_linkset();
	void *mem_end = sbrk(0);
	loader_add_starting_symbols(linkset, sizeof(symbols) / sizeof(*symbols), symbols);

	/* Register upload channel -1 */
	mux_channel_add(-1, 65536, true);

	/* Signal ready to the host upload tool */
	mux_send(-1, "READY\n", 6);

	/* Kernel-phase: receive files until an empty separator line */
	for (;;)
	{
		char path[256];
		if (recv_line(-1, path, sizeof(path)) < 0)
			return -1;
		if (path[0] == '\0')
		{
			/* Separator: finish the kernel-phase link */
			if (loader_finish_link(linkset) != LOADER_ERROR_NONE)
			{
				fputs("Linking failed!\n", stdout);
				return -1;
			}
			break;
		}

		char size_line[32];
		if (recv_line(-1, size_line, sizeof(size_line)) < 0)
			return -1;
		size_t size = (size_t)strtoul(size_line, NULL, 10);
		void *buf = malloc(size);
		if (!buf)
		{
			fputs("Out of memory receiving file\n", stdout);
			return -1;
		}
		if (recv_n(-1, buf, size) != 0)
			return -1;

		FILE *f = fmemopen(buf, size, "rb");
		if (!f)
		{
			fprintf(stdout, "Failed to open received payload %s\n", path);
			free(buf);
			continue;
		}
		loader_read_file(linkset, f, path);
		fclose(f);
		/* Acknowledge receipt */
		mux_send(-1, "ACK\n", 4);
		/* keep buffer around until after finish_link (loader may copy or reference) */
	}

	/* Extra-phase: receive files one-by-one, finish link after each */
	for (;;)
	{
		char path[256];
		if (recv_line(-1, path, sizeof(path)) < 0)
			return -1;
		if (strcmp(path, "DONE") == 0)
			break;
		if (path[0] == '\0')
			continue;

		char size_line[32];
		if (recv_line(-1, size_line, sizeof(size_line)) < 0)
			return -1;
		size_t size = (size_t)strtoul(size_line, NULL, 10);
		void *buf = malloc(size);
		if (!buf)
		{
			fputs("Out of memory receiving file\n", stdout);
			return -1;
		}
		if (recv_n(-1, buf, size) != 0)
			return -1;

		FILE *f = fmemopen(buf, size, "rb");
		if (!f)
		{
			fprintf(stdout, "Failed to open received payload %s\n", path);
			free(buf);
			continue;
		}
		loader_read_file(linkset, f, path);
		fclose(f);
		if (loader_finish_link(linkset) != LOADER_ERROR_NONE)
		{
			fputs("Linking failed!\n", stdout);
			return -1;
		}
		mux_send(-1, "ACK\n", 4);
	}

	loader_print_tls_layout(linkset);
	tcb = loader_create_tcb(linkset);

	boot_info = (struct boot_info){
		.boot_memory_end = mem_end,
		.memory_start = sbrk(0),
		.memory_end = (void *)0x3E000000,
	};
	boot_data = (struct boot_customdata){
		.test_function = loader_search_symbol(linkset, "test_function_02"),
		.module_data = loader_tls_ptr(tcb, (ssize_t)loader_search_symbol(linkset, "module_data")),
	};
	boot_userdata.custom = &boot_data;
	kernel_start = loader_search_symbol(linkset, "_start");

	fputs("Jumping to kernel...\n", stdout);

	return 0;
}
