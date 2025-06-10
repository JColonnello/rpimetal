#pragma once

#include <stddef.h>
#include <stdio.h>

#define CONCAT(p1, p2) p1##p2
#define EVALUATOR(p1, p2) CONCAT(p1, p2)
#define _BINARY_SYMBOL_PREFIX(SYMBOL) CONCAT(_binary_build_modules_, SYMBOL)
#define _BINARY_START(NAME) EVALUATOR(_BINARY_SYMBOL_PREFIX(NAME), _ko_start)
#define _BINARY_END(NAME) EVALUATOR(_BINARY_SYMBOL_PREFIX(NAME), _ko_end)
#define FILE_FROM_SYMBOL_FUNC_CALL(NAME) _##NAME##_get_file()
#define FILE_FROM_SYMBOL_FUNC_DECL(NAME) \
	extern char _BINARY_START(NAME)[], _BINARY_END(NAME)[]; \
	FILE *_##NAME##_get_file() \
	{ \
		return fmemopen(_BINARY_START(NAME), (size_t)(_BINARY_END(NAME) - _BINARY_START(NAME)), "rb"); \
	}

typedef struct symbol_data
{
	const char *name;
	void *address;
} symbol_data;
struct tls_data;
struct tls_info;
extern struct tls_info *tls_schema;

unsigned int loader_init();
void loader_add_starting_symbols(size_t n, const symbol_data symbols[n]);
int loader_load_file(FILE *file, const char *filename);
void *loader_search_symbol(const char *name);
inline void *local_tls_offset(void *var)
{
	return (void *)(var - __builtin_thread_pointer());
}
struct tls_data *loader_create_tcb();
struct tls_data *loader_switch_tcb(struct tls_data *tcb);
void *loader_tls_ptr(const struct tls_data *tcb, ssize_t offset);
void loader_print_tls_layout(const struct tls_info *schema);
