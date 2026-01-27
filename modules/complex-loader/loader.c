#include "_loader.h"
#include "complex-loader.h"
#include <bfd.h>
#include <sglib.h>
#include <stddef.h>
#include <stdlib.h>

#pragma region Type definitions

typedef struct __attribute__((packed)) tls_data
{
	uint64_t _res1;
	uint32_t thread_id;
	uint32_t _res2;
	// unsigned int data_size;
	uint64_t data[];
} tls_data;

_Static_assert(sizeof(tls_data) == 16, "TLS control block is not 16 bytes long");
struct tcb_instance
{
	void *address;
	struct tcb_instance *next;
};

struct linkset
{
	struct tls_data *tls_template;
	struct tls_info *tls_schema;
	struct tcb_instance *tcbs;
	struct assembly_data *loaded_assemblies;
	struct tructor_data *constructors;
	struct tructor_data *destructors;
};

static section_data *absolute_section = &(section_data){
	.name = "<absolute>",
	.address = NULL,
	.tls = false,
	.next = NULL,
	.bfd_section = bfd_abs_section_ptr,
};

#define NAME_COMP(x, y) \
	((x)->name == NULL || (y)->name == NULL ? SGLIB_SAFE_NUMERIC_COMPARATOR((x)->name, (y)->name) \
											: strcmp((x)->name, (y)->name))
#define COMP_DIRECT(comp, x, y) comp(&x, &y)
#define NAME_COMP_DIRECT(x, y) COMP_DIRECT(NAME_COMP, x, y)

SGLIB_DEFINE_LIST_PROTOTYPES(assembly_data, NAME_COMP, next)
SGLIB_DEFINE_LIST_FUNCTIONS(assembly_data, NAME_COMP, next)
SGLIB_DEFINE_LIST_PROTOTYPES(section_data, NAME_COMP, next)
SGLIB_DEFINE_LIST_FUNCTIONS(section_data, NAME_COMP, next)
SGLIB_DEFINE_LIST_PROTOTYPES(tructor_data, SGLIB_NUMERIC_COMPARATOR, next)
SGLIB_DEFINE_LIST_FUNCTIONS(tructor_data, SGLIB_NUMERIC_COMPARATOR, next)
SGLIB_DEFINE_LIST_PROTOTYPES(tls_info, SGLIB_NUMERIC_COMPARATOR, next)
SGLIB_DEFINE_LIST_FUNCTIONS(tls_info, SGLIB_NUMERIC_COMPARATOR, next)

static void *section_malloc(size_t size)
{
	return malloc(size);
}
static void section_free(void *ptr)
{
	free(ptr);
}

static void *section_realloc(void *ptr, size_t size)
{
	return realloc(ptr, size);
}