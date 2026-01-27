#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#ifndef LOADER_SYMBOL_BIND
#define LOADER_SYMBOL_BIND
enum symbol_bind
{
	SYMBOL_BIND_GLOBAL,
	SYMBOL_BIND_LOCAL,
	SYMBOL_BIND_WEAK,
};
#endif

struct start_symbol
{
	const char *name;
	void *address;
	enum symbol_bind type;
};

struct linkset;

enum loader_error
{
	LOADER_ERROR_NONE = 0,
	LOADER_ERROR_INVALID_FILE,
	LOADER_ERROR_OUT_OF_MEMORY,
};

struct linkset *loader_create_linkset();
void loader_free_linkset(struct linkset *linkset);
void loader_add_starting_symbols(struct linkset *linkset, size_t n, const struct start_symbol symbols[n]);
enum loader_error loader_read_file(struct linkset *linkset, FILE *file, const char *filename);
enum loader_error loader_finish_link(struct linkset *linkset);
void *loader_search_symbol(struct linkset *linkset, const char *name);
inline void *local_tls_offset(void *var)
{
	return (void *)(var - __builtin_thread_pointer());
}
struct tls_data *loader_create_tcb(struct linkset *linkset);
inline void *loader_tls_ptr(const struct tls_data *tcb, ssize_t offset)
{
	return (void *)tcb + offset;
}
struct tls_data *loader_switch_tcb(struct tls_data *tcb);
void loader_print_tls_layout(const struct linkset *linkset);
