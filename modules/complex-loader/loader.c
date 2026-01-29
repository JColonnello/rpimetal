#define LOADER_INLINE_DEFINITIONS
#include "_loader.h"
#include "attrib.h"
#include "complex-loader.h"
#include "relocation.h"
#include <bfd.h>
#include <sglib.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma region Type definitions

typedef struct __attribute__((packed)) tls_data
{
	uint64_t _res1;
	uint32_t thread_id;
	uint32_t _res2;
	uint64_t data[];
} tls_data;

static_assert(sizeof(tls_data) == 16, "TLS control block is not 16 bytes long");

typedef struct tcb_instance
{
	void *address;
	struct tcb_instance *next;
} tcb_instance;

struct linkset
{
	struct tls_data *tls_template;
	struct tls_info *tls_schema;
	struct tcb_instance *tcbs;
	struct assembly_data *loaded_assemblies;
	struct tructor_data *constructors;
	struct tructor_data *destructors;
	void *constructor_got[6];
	uint16_t undefined_offset;
};

static section_data *absolute_section = &(section_data){
	.name = "<absolute>",
	.address = NULL,
	.size = 0,
	.tls = false,
	.next = NULL,
	.bfd_section = bfd_abs_section_ptr,
	.assembly = NULL,
};
static section_data *undefined_section = &(section_data){
	.name = "<undefined>",
	.address = NULL, //(void *)(0xDEADBEAFL << 32),
	.size = 0,
	.tls = false,
	.next = NULL,
	.bfd_section = bfd_und_section_ptr,
	.assembly = NULL,
};

#define NAME_COMP(x, y) \
	((x)->name == NULL || (y)->name == NULL ? SGLIB_SAFE_NUMERIC_COMPARATOR((x)->name, (y)->name) \
											: strcmp((x)->name, (y)->name))
#define COMP_DIRECT(comp, x, y) comp(&x, &y)
#define NAME_COMP_DIRECT(x, y) COMP_DIRECT(NAME_COMP, x, y)
#define PRIO_COMP(x, y) SGLIB_NUMERIC_COMPARATOR((x)->priority, (y)->priority)
#define OFFSET_COMP(x, y) SGLIB_NUMERIC_COMPARATOR((y)->offset, (x)->offset)

SGLIB_DEFINE_LIST_PROTOTYPES(assembly_data, NAME_COMP, next)
SGLIB_DEFINE_LIST_FUNCTIONS(assembly_data, NAME_COMP, next)
SGLIB_DEFINE_LIST_PROTOTYPES(section_data, NAME_COMP, next)
SGLIB_DEFINE_LIST_FUNCTIONS(section_data, NAME_COMP, next)
SGLIB_DEFINE_LIST_PROTOTYPES(tructor_data, PRIO_COMP, next)
SGLIB_DEFINE_LIST_FUNCTIONS(tructor_data, PRIO_COMP, next)
SGLIB_DEFINE_LIST_PROTOTYPES(tls_info, OFFSET_COMP, next)
SGLIB_DEFINE_LIST_FUNCTIONS(tls_info, OFFSET_COMP, next)
SGLIB_DEFINE_LIST_PROTOTYPES(tcb_instance, SGLIB_NUMERIC_COMPARATOR, next)
SGLIB_DEFINE_LIST_FUNCTIONS(tcb_instance, SGLIB_NUMERIC_COMPARATOR, next)
SGLIB_DEFINE_ARRAY_SORTING_PROTOTYPES(symbol_data, NAME_COMP_DIRECT)
SGLIB_DEFINE_ARRAY_SORTING_FUNCTIONS(symbol_data, NAME_COMP_DIRECT)

static void *section_malloc(size_t size, size_t alignment)
{
	if (alignment < sizeof(void *))
		alignment = sizeof(void *);
	void *ptr = aligned_alloc(alignment, size);
	if (ptr == NULL)
		return NULL;
	return ptr;
}

static void section_free(void *ptr)
{
	free(ptr);
}

__attribute__((unused)) static void *section_realloc(void *ptr, size_t size)
{
	return realloc(ptr, size);
}

#pragma endregion

static constructor void loader_init()
{
	bfd_init();
}

#pragma region Linkset Management

struct linkset *loader_create_linkset(void)
{
	struct linkset *linkset = calloc(1, sizeof(struct linkset));
	if (linkset == NULL)
		return NULL;

	assembly_data *start = calloc(1, sizeof(assembly_data));
	if (start == NULL)
	{
		free(linkset);
		return NULL;
	}
	sglib_assembly_data_add(&linkset->loaded_assemblies, start);

	return linkset;
}

static void free_section_data(section_data *sections)
{
	struct sglib_section_data_iterator it;
	for (section_data *s = sglib_section_data_it_init(&it, sections); s != NULL; s = sglib_section_data_it_next(&it))
	{
		free((void *)s->name);
		if (s->allocated && s->address != NULL)
			section_free(s->address);
		free(s);
	}
}

static void free_assembly_data(assembly_data *assembly)
{
	if (assembly == NULL)
		return;

	free((void *)assembly->name);

	for (int i = 0; i < assembly->symbol_count; i++)
		free((void *)assembly->symbols[i].name);
	free(assembly->symbols);

	free_section_data(assembly->sections);

	if (assembly->bfd_file != NULL)
		bfd_close(assembly->bfd_file);

	free(assembly);
}

void loader_free_linkset(struct linkset *linkset)
{
	if (linkset == NULL)
		return;

	struct sglib_assembly_data_iterator ait;
	for (assembly_data *a = sglib_assembly_data_it_init(&ait, linkset->loaded_assemblies); a != NULL;)
	{
		assembly_data *next = sglib_assembly_data_it_next(&ait);
		free_assembly_data(a);
		a = next;
	}

	struct sglib_tructor_data_iterator tit;
	for (tructor_data *t = sglib_tructor_data_it_init(&tit, linkset->constructors); t != NULL;)
	{
		tructor_data *next = sglib_tructor_data_it_next(&tit);
		free(t);
		t = next;
	}
	for (tructor_data *t = sglib_tructor_data_it_init(&tit, linkset->destructors); t != NULL;)
	{
		tructor_data *next = sglib_tructor_data_it_next(&tit);
		free(t);
		t = next;
	}

	struct sglib_tls_info_iterator tlsit;
	for (tls_info *t = sglib_tls_info_it_init(&tlsit, linkset->tls_schema); t != NULL;)
	{
		tls_info *next = sglib_tls_info_it_next(&tlsit);
		free(t);
		t = next;
	}

	struct sglib_tcb_instance_iterator tcbit;
	for (struct tcb_instance *t = sglib_tcb_instance_it_init(&tcbit, linkset->tcbs); t != NULL;)
	{
		struct tcb_instance *next = sglib_tcb_instance_it_next(&tcbit);
		free(t->address);
		free(t);
		t = next;
	}

	free(linkset->tls_template);
	free(linkset);
}

#pragma endregion

#pragma region Symbol Management

void loader_add_starting_symbols(struct linkset *linkset, size_t n, const struct start_symbol symbols[n])
{
	if (linkset == NULL || n == 0)
		return;

	assembly_data *match, key = {.name = NULL};
	match = sglib_assembly_data_find_member(linkset->loaded_assemblies, &key);
	if (match == NULL)
		return;

	size_t count = match->symbol_count;
	symbol_data *new_symbols = realloc(match->symbols, (count + n) * sizeof(symbol_data));
	if (new_symbols == NULL)
		return;
	match->symbols = new_symbols;

	for (size_t i = 0; i < n; i++)
	{
		match->symbols[count + i] = (symbol_data){
			.name = strdup(symbols[i].name),
			.value = (uint64_t)symbols[i].address,
			.type = symbols[i].type,
			.section = absolute_section,
		};
	}
	match->symbol_count = count + n;
	sglib_symbol_data_array_quick_sort(match->symbols, match->symbol_count);
}

static const symbol_data *search_symbol(struct linkset *linkset, const char *name)
{
#define SEARCH_FUNC(p, k) strcmp((&p)->name, k)

	struct sglib_assembly_data_iterator it;
	bool found = false;
	int index = -1;
	const symbol_data *weak_match = NULL;

	for (assembly_data *assembly = sglib_assembly_data_it_init(&it, linkset->loaded_assemblies); assembly != NULL;
		 assembly = sglib_assembly_data_it_next(&it))
	{
		if (assembly->symbol_count == 0)
			continue;
		SGLIB_ARRAY_BINARY_SEARCH(
			symbol_data, assembly->symbols, 0, assembly->symbol_count - 1, name, SEARCH_FUNC, found, index
		);
		if (found)
		{
			const symbol_data *sym = &assembly->symbols[index];
			if (sym->type == SYMBOL_BIND_GLOBAL)
				return sym;
			if (sym->type == SYMBOL_BIND_WEAK)
				weak_match = sym;
		}
	}
	return weak_match;

#undef SEARCH_FUNC
}

void *loader_search_symbol(struct linkset *linkset, const char *name)
{
	const symbol_data *sym = search_symbol(linkset, name);
	if (sym != NULL)
		return sym->value + sym->section->address;
	return NULL;
}

#pragma endregion

#pragma region TLS Management

static ssize_t register_tls_segment(struct linkset *linkset, size_t size, size_t alignment, section_data *section)
{
	size_t current_size =
		linkset->tls_schema != NULL ? linkset->tls_schema->offset + linkset->tls_schema->size : sizeof(tls_data);

	ssize_t offset = current_size;
	ssize_t extra = offset % alignment;
	if (extra > 0)
		extra = alignment - extra;

	if (extra > 0)
	{
		tls_info *padding = malloc(sizeof(tls_info));
		if (padding == NULL)
			return -1;
		*padding = (tls_info){
			.in_use = false,
			.offset = offset,
			.size = extra,
			.section = NULL,
		};
		sglib_tls_info_add(&linkset->tls_schema, padding);
		offset += extra;
	}

	tls_info *entry = malloc(sizeof(tls_info));
	if (entry == NULL)
		return -1;
	*entry = (tls_info){
		.in_use = true,
		.offset = offset,
		.size = size,
		.section = section,
	};
	sglib_tls_info_add(&linkset->tls_schema, entry);

	void *new_template = realloc(linkset->tls_template, offset + size);
	if (new_template == NULL)
		return -1;
	linkset->tls_template = new_template;

	return offset;
}

struct tls_data *loader_create_tcb(struct linkset *linkset)
{
	if (linkset == NULL || linkset->tls_schema == NULL)
		return NULL;

	size_t size = linkset->tls_schema->offset + linkset->tls_schema->size;
	tls_data *tcb = malloc(size);
	if (tcb == NULL)
		return NULL;

	memcpy(tcb, linkset->tls_template, size);

	struct tcb_instance *instance = malloc(sizeof(struct tcb_instance));
	if (instance != NULL)
	{
		instance->address = tcb;
		sglib_tcb_instance_add(&linkset->tcbs, instance);
	}

	return tcb;
}

struct tls_data *loader_switch_tcb(struct tls_data *tcb)
{
	struct tls_data *tmp;
	asm("mrs %0, tpidr_el1" : "=r"(tmp));
	if (tcb != NULL)
		asm("msr tpidr_el1, %0" : : "r"(tcb));
	return tmp;
}

void loader_print_tls_layout(const struct linkset *linkset)
{
	printf("TLS Schema:\n");
	const assembly_data *last = NULL;
	SGLIB_LIST_MAP_ON_ELEMENTS(const tls_info, linkset->tls_schema, segment, next, {
		if (segment->in_use)
		{
			if (last != segment->section->assembly)
			{
				last = segment->section->assembly;
				printf("Assembly: %s\n", last->name != NULL ? last->name : "LOCAL");
			}
			printf("\tOffset = %u, Size = %u, Section: %s\n", segment->offset, segment->size, segment->section->name);
		}
		else
			printf("\tAlignment = %u\n", segment->size);
	});
}

void *local_tls_offset(void *var)
{
	return (void *)(var - __builtin_thread_pointer());
}
void *loader_tls_ptr(const struct tls_data *tcb, ssize_t offset)
{
	return (void *)tcb + offset;
}

#pragma endregion

#pragma region File Loading

static bool is_constructor_section(const char *name, uint16_t *priority)
{
	if (strcmp(name, ".init_array") == 0)
	{
		*priority = 65535;
		return true;
	}
	if (strncmp(name, ".init_array.", 12) == 0)
	{
		*priority = atoi(name + 12);
		return true;
	}
	if (strcmp(name, ".ctors") == 0)
	{
		*priority = 65535;
		return true;
	}
	return false;
}

static bool is_destructor_section(const char *name, uint16_t *priority)
{
	if (strcmp(name, ".fini_array") == 0)
	{
		*priority = 65535;
		return true;
	}
	if (strncmp(name, ".fini_array.", 12) == 0)
	{
		*priority = atoi(name + 12);
		return true;
	}
	if (strcmp(name, ".dtors") == 0)
	{
		*priority = 65535;
		return true;
	}
	return false;
}

enum loader_error loader_read_file(struct linkset *linkset, FILE *file, const char *filename)
{
	if (linkset == NULL || file == NULL)
		return LOADER_ERROR_INVALID_FILE;

	bfd *abfd = bfd_openstreamr(filename, NULL, file);
	if (abfd == NULL)
		return LOADER_ERROR_INVALID_FILE;

	if (!bfd_check_format(abfd, bfd_object))
	{
		bfd_close(abfd);
		return LOADER_ERROR_INVALID_FILE;
	}

	assembly_data *assembly = calloc(1, sizeof(assembly_data));
	if (assembly == NULL)
	{
		bfd_close(abfd);
		return LOADER_ERROR_OUT_OF_MEMORY;
	}
	assembly->name = strdup(filename);
	assembly->bfd_file = abfd;
	assembly->loaded = false;

	section_data *loaded_sections = NULL;

	for (asection *section = abfd->sections; section != NULL; section = section->next)
	{
		flagword flags = section->flags;
		if (!(flags & SEC_ALLOC))
			continue;

		section_data *sec = calloc(1, sizeof(section_data));
		if (sec == NULL)
		{
			free_section_data(loaded_sections);
			free_assembly_data(assembly);
			return LOADER_ERROR_OUT_OF_MEMORY;
		}

		sec->name = strdup(section->name);
		sec->size = section->size;
		sec->tls = !!(flags & SEC_THREAD_LOCAL);
		sec->bfd_section = section;
		sec->assembly = assembly;

		// Check for constructor/destructor sections
		uint16_t priority;
		if (is_constructor_section(section->name, &priority))
		{
			tructor_data *ctor = malloc(sizeof(tructor_data));
			if (ctor != NULL)
			{
				ctor->priority = priority;
				ctor->section = sec;
				sglib_tructor_data_add(&linkset->constructors, ctor);
			}
		}
		else if (is_destructor_section(section->name, &priority))
		{
			tructor_data *dtor = malloc(sizeof(tructor_data));
			if (dtor != NULL)
			{
				dtor->priority = priority;
				dtor->section = sec;
				sglib_tructor_data_add(&linkset->destructors, dtor);
			}
		}

		sglib_section_data_add(&loaded_sections, sec);
	}

	// Load symbol table
	size_t symsize = bfd_get_symtab_upper_bound(abfd);
	asymbol **bfd_symbols = malloc(symsize);
	if (bfd_symbols == NULL)
	{
		free_section_data(loaded_sections);
		free_assembly_data(assembly);
		return LOADER_ERROR_OUT_OF_MEMORY;
	}

	int symcount = bfd_canonicalize_symtab(abfd, bfd_symbols);
	symbol_data *symbols = malloc(sizeof(symbol_data) * symcount);
	if (symbols == NULL)
	{
		free(bfd_symbols);
		free_section_data(loaded_sections);
		free_assembly_data(assembly);
		return LOADER_ERROR_OUT_OF_MEMORY;
	}

	size_t export_count = 0;
	for (int i = 0; i < symcount; i++)
	{
		asymbol *sym = bfd_symbols[i];
		if (bfd_is_und_section(sym->section))
			continue;

		enum symbol_bind bind;
		if (sym->flags & BSF_GLOBAL)
			bind = SYMBOL_BIND_GLOBAL;
		else if (sym->flags & BSF_WEAK)
			bind = SYMBOL_BIND_WEAK;
		else
			continue;

		section_data *sec_match = NULL;
		if (!bfd_is_abs_section(sym->section))
		{
			section_data key = {.name = (char *)sym->section->name};
			sec_match = sglib_section_data_find_member(loaded_sections, &key);
			assert(sec_match != NULL);
		}
		else
		{
			sec_match = absolute_section;
		}

		symbols[export_count++] = (symbol_data){
			.name = strdup(sym->name),
			.value = sym->value,
			.type = bind,
			.section = sec_match,
		};
	}

	free(bfd_symbols);
	symbols = realloc(symbols, sizeof(symbol_data) * export_count);
	sglib_symbol_data_array_quick_sort(symbols, export_count);

	assembly->sections = loaded_sections;
	assembly->symbol_count = export_count;
	assembly->symbols = symbols;

	sglib_assembly_data_add(&linkset->loaded_assemblies, assembly);

	return LOADER_ERROR_NONE;
}

#pragma endregion

#pragma region Linking

static size_t build_tructor_array(tructor_data *list, void **out_buffer)
{
	size_t total_bytes = 0;
	/* compute total size */
	struct sglib_tructor_data_iterator _it;
	for (tructor_data *t = sglib_tructor_data_it_init(&_it, list); t != NULL; t = sglib_tructor_data_it_next(&_it))
		total_bytes += t->section != NULL ? t->section->size : 0;

	void *buffer = NULL;
	if (total_bytes > 0)
	{
		buffer = section_malloc(total_bytes, sizeof(void *));
		if (buffer == NULL)
		{
			fprintf(stderr, "Failed to allocate tructor array\n");
			return 0;
		}

		/* set sections addresses in list order into buffer */
		size_t pos = 0;
		for (tructor_data *t = sglib_tructor_data_it_init(&_it, list); t != NULL; t = sglib_tructor_data_it_next(&_it))
		{
			if (t->section == NULL)
				continue;
			t->section->address = buffer + pos;
			pos += t->section->size;
		}
		// Mark only the first section as allocated to avoid double free
		list->section->allocated = true;
	}

	*out_buffer = buffer;
	return total_bytes;
}

static void create_tructor_array(struct linkset *linkset)
{
	if (linkset == NULL)
		return;

	// Sort constructor and destructor lists (sglib-generated sort function)
	sglib_tructor_data_sort(&linkset->constructors);
	sglib_tructor_data_sort(&linkset->destructors);
	sglib_tructor_data_reverse(&linkset->destructors);

	// Position tructor sections in memory
	void *init_array, *fini_array;
	size_t init_size = build_tructor_array(linkset->constructors, &init_array);
	size_t fini_size = build_tructor_array(linkset->destructors, &fini_array);

	// Set up GOT for constructors
	linkset->constructor_got[0] = init_array;             // __init_array_start
	linkset->constructor_got[1] = init_array + init_size; // __init_array_end
	linkset->constructor_got[2] = fini_array;             // __fini_array_start
	linkset->constructor_got[3] = fini_array + fini_size; // __fini_array_end
	linkset->constructor_got[4] = NULL;                   // __preinit_array_start
	linkset->constructor_got[5] = NULL;                   // __preinit_array_end

	// Add start/end symbols
	struct start_symbol syms[] = {
		{.name = "__init_array_start", .address = &linkset->constructor_got[0], .type = SYMBOL_BIND_GLOBAL},
		{.name = "__init_array_end", .address = &linkset->constructor_got[1], .type = SYMBOL_BIND_GLOBAL},
		{.name = "__fini_array_start", .address = &linkset->constructor_got[2], .type = SYMBOL_BIND_GLOBAL},
		{.name = "__fini_array_end", .address = &linkset->constructor_got[3], .type = SYMBOL_BIND_GLOBAL},
		{.name = "__preinit_array_start", .address = &linkset->constructor_got[4], .type = SYMBOL_BIND_GLOBAL},
		{.name = "__preinit_array_end", .address = &linkset->constructor_got[5], .type = SYMBOL_BIND_GLOBAL},
	};
	loader_add_starting_symbols(linkset, sizeof(syms) / sizeof(*syms), syms);
}

static void allocate_sections(struct linkset *linkset)
{
	if (!(linkset->constructors->section->allocated || linkset->destructors->section->allocated))
		create_tructor_array(linkset);

	struct sglib_assembly_data_iterator ait;
	for (assembly_data *assembly = sglib_assembly_data_it_init(&ait, linkset->loaded_assemblies); assembly != NULL;
		 assembly = sglib_assembly_data_it_next(&ait))
	{
		if (assembly->loaded)
			continue;

		struct sglib_section_data_iterator sit;
		for (section_data *section = sglib_section_data_it_init(&sit, assembly->sections); section != NULL;
			 section = sglib_section_data_it_next(&sit))
		{

			void *target;
			if (section->address != NULL)
				target = section->address;
			else if (section->tls)
			{
				ssize_t offset =
					register_tls_segment(linkset, section->size, 1 << section->bfd_section->alignment_power, section);
				assert(offset >= 0); // TODO: Error handling
				section->address = (void *)(uintptr_t)offset;
				target = (char *)linkset->tls_template + (uintptr_t)section->address;
			}
			else
			{
				void *addr = section_malloc(section->size, 1 << section->bfd_section->alignment_power);
				assert(addr != NULL); // TODO: Error handling
				target = section->address = addr;
				section->allocated = true;
			}

			if (section->bfd_section->flags & SEC_LOAD)
				bfd_get_section_contents(section->bfd_section->owner, section->bfd_section, target, 0, section->size);
			else
				memset(target, 0, section->size);
		}
	}
}

enum loader_error loader_finish_link(struct linkset *linkset)
{
	if (linkset == NULL)
		return LOADER_ERROR_INVALID_FILE;

	allocate_sections(linkset);

	struct sglib_assembly_data_iterator ait;
	for (assembly_data *assembly = sglib_assembly_data_it_init(&ait, linkset->loaded_assemblies); assembly != NULL;
		 assembly = sglib_assembly_data_it_next(&ait))
	{
		if (assembly->loaded || assembly->bfd_file == NULL)
			continue;

		bfd *abfd = assembly->bfd_file;

		// Get symbols for relocation
		size_t symsize = bfd_get_symtab_upper_bound(abfd);
		asymbol **bfd_symbols = malloc(symsize);
		if (bfd_symbols == NULL)
			return LOADER_ERROR_OUT_OF_MEMORY;

		bfd_canonicalize_symtab(abfd, bfd_symbols);

		// Process each section
		struct sglib_section_data_iterator sit;
		for (section_data *sec = sglib_section_data_it_init(&sit, assembly->sections); sec != NULL;
			 sec = sglib_section_data_it_next(&sit))
		{
			asection *bfd_section = sec->bfd_section;
			if (bfd_section == NULL)
				continue;

			long relsize = bfd_get_reloc_upper_bound(abfd, bfd_section);
			if (relsize <= 0)
				continue;

			arelent **relocs = malloc(relsize);
			if (relocs == NULL)
			{
				free(bfd_symbols);
				return LOADER_ERROR_OUT_OF_MEMORY;
			}

			long relcount = bfd_canonicalize_reloc(abfd, bfd_section, relocs, bfd_symbols);

			for (long i = 0; i < relcount; i++)
			{
				arelent *b_reloc = relocs[i];
				asymbol *b_sym = *b_reloc->sym_ptr_ptr;

				uint64_t target_value = b_sym->value;
				section_data *target_section = NULL;
				uint64_t addend = b_reloc->addend;

				if (!(b_sym->flags & BSF_LOCAL))
				{
					// Look up in linkset
					const symbol_data *found = search_symbol(linkset, b_sym->name);
					if (found != NULL)
					{
						target_value = found->value;
						target_section = found->section;
					}
					else if (b_sym->flags & BSF_WEAK)
						continue;
					else
					{
						target_section = undefined_section;
						target_value = linkset->undefined_offset;
						addend = 0;
						linkset->undefined_offset += 8;
						// fprintf(stderr, "Undefined symbol `%s` pointed to 0x%04lx\n", b_sym->name, target_value);
					}
				}
				else
				{
					// Local section symbol
					if (bfd_is_abs_section(b_sym->section))
						target_section = absolute_section;
					else
					{
						section_data key = {.name = (char *)b_sym->section->name};
						section_data *local_sec = sglib_section_data_find_member(assembly->sections, &key);
						assert(local_sec != NULL);
						target_section = local_sec;
					}
				}

				uintptr_t offset = sec->tls ? (uintptr_t)linkset->tls_template + b_reloc->address : b_reloc->address;

				bfd_reloc_status_type status =
					aarch64_relocate(b_reloc->howto->type, abfd, sec, target_section, offset, target_value, addend);

				if (status == bfd_reloc_dangerous)
					printf("Dangerous relocation\n");
				else if (status != bfd_reloc_ok && status != bfd_reloc_undefined)
				{
					printf("Failed relocation for symbol `%s`. Error %d\n", b_sym->name, status);
					free(relocs);
					free(bfd_symbols);
					return LOADER_ERROR_RELOCATION_FAILED;
				}
			}

			free(relocs);
		}

		free(bfd_symbols);
		assembly->loaded = true;
	}

	return LOADER_ERROR_NONE;
}

#pragma endregion