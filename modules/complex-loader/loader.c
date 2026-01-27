#include "_loader.h"
#include "complex-loader.h"
#include "relocation.h"
#include <bfd.h>
#include <sglib.h>
#include <stddef.h>
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

_Static_assert(sizeof(tls_data) == 16, "TLS control block is not 16 bytes long");

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
SGLIB_DEFINE_LIST_PROTOTYPES(tcb_instance, SGLIB_NUMERIC_COMPARATOR, next)
SGLIB_DEFINE_LIST_FUNCTIONS(tcb_instance, SGLIB_NUMERIC_COMPARATOR, next)

static void *section_malloc(size_t size, size_t alignment)
{
	if (alignment < sizeof(void *))
		alignment = sizeof(void *);
	void *ptr = NULL;
	if (posix_memalign(&ptr, alignment, size) != 0)
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

#pragma region Linkset Management

struct linkset *loader_create_linkset(void)
{
	bfd_init();

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
		if (!s->tls && s->address != NULL)
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
			.value = symbols[i].address,
			.type = symbols[i].type,
			.section = absolute_section,
		};
	}
	match->symbol_count = count + n;
	SGLIB_ARRAY_SINGLE_QUICK_SORT(symbol_data, match->symbols, match->symbol_count, NAME_COMP_DIRECT);
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
			if (sym->type == SYMBOL_BIND_WEAK && weak_match == NULL)
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
		return sym->value;
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
		SGLIB_LIST_ADD(tls_info, linkset->tls_schema, padding, next);
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
	SGLIB_LIST_ADD(tls_info, linkset->tls_schema, entry, next);

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
		SGLIB_LIST_ADD(tcb_instance, linkset->tcbs, instance, next);
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
	if (linkset == NULL)
		return;

	printf("TLS Schema:\n");
	const section_data *last_section = NULL;
	SGLIB_LIST_MAP_ON_ELEMENTS(const tls_info, linkset->tls_schema, segment, next, {
		if (segment->in_use)
		{
			if (segment->section != NULL && last_section != segment->section)
			{
				last_section = segment->section;
				const char *asm_name = (last_section->assembly != NULL && last_section->assembly->name != NULL)
										   ? last_section->assembly->name
										   : "LOCAL";
				printf("Assembly: %s, Section: %s\n", asm_name, last_section->name);
			}
			printf("\tOffset = %u, Size = %u\n", segment->offset, segment->size);
		}
		else
			printf("\tAlignment padding = %u\n", segment->size);
	});
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

	// First pass: allocate memory for sections
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

		if (flags & SEC_THREAD_LOCAL)
		{
			ssize_t offset = register_tls_segment(linkset, section->size, 1 << section->alignment_power, sec);
			if (offset < 0)
			{
				free((void *)sec->name);
				free(sec);
				free_section_data(loaded_sections);
				free_assembly_data(assembly);
				return LOADER_ERROR_OUT_OF_MEMORY;
			}
			sec->address = (void *)(intptr_t)offset;
		}
		else
		{
			size_t alignment = 1 << section->alignment_power;
			sec->address = section_malloc(section->size, alignment);
			if (sec->address == NULL)
			{
				free((void *)sec->name);
				free(sec);
				free_section_data(loaded_sections);
				free_assembly_data(assembly);
				return LOADER_ERROR_OUT_OF_MEMORY;
			}
		}

		if (flags & SEC_LOAD)
		{
			void *target = sec->tls ? ((char *)linkset->tls_template + (intptr_t)sec->address) : sec->address;
			bfd_get_section_contents(abfd, section, target, 0, section->size);
		}
		else
		{
			void *target = sec->tls ? ((char *)linkset->tls_template + (intptr_t)sec->address) : sec->address;
			memset(target, 0, section->size);
		}

		// Check for constructor/destructor sections
		uint16_t priority;
		if (is_constructor_section(section->name, &priority))
		{
			tructor_data *ctor = malloc(sizeof(tructor_data));
			if (ctor != NULL)
			{
				ctor->priority = priority;
				ctor->section = sec;
				SGLIB_LIST_ADD(tructor_data, linkset->constructors, ctor, next);
			}
		}
		else if (is_destructor_section(section->name, &priority))
		{
			tructor_data *dtor = malloc(sizeof(tructor_data));
			if (dtor != NULL)
			{
				dtor->priority = priority;
				dtor->section = sec;
				SGLIB_LIST_ADD(tructor_data, linkset->destructors, dtor, next);
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
		}

		void *sym_value;
		if (sec_match != NULL)
		{
			if (sec_match->tls)
				sym_value = (void *)((intptr_t)sec_match->address + sym->value);
			else
				sym_value = (char *)sec_match->address + sym->value;
		}
		else
			sym_value = (void *)sym->value;

		symbols[export_count++] = (symbol_data){
			.name = strdup(sym->name),
			.value = sym_value,
			.type = bind,
			.section = sec_match != NULL ? sec_match : absolute_section,
		};
	}

	free(bfd_symbols);
	symbols = realloc(symbols, sizeof(symbol_data) * export_count);
	SGLIB_ARRAY_SINGLE_QUICK_SORT(symbol_data, symbols, export_count, NAME_COMP_DIRECT);

	assembly->sections = loaded_sections;
	assembly->symbol_count = export_count;
	assembly->symbols = symbols;

	sglib_assembly_data_add(&linkset->loaded_assemblies, assembly);

	return LOADER_ERROR_NONE;
}

#pragma endregion

#pragma region Linking

enum loader_error loader_finish_link(struct linkset *linkset)
{
	if (linkset == NULL)
		return LOADER_ERROR_INVALID_FILE;

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
				arelent *reloc = relocs[i];
				asymbol *sym = *reloc->sym_ptr_ptr;

				// Find the target symbol value
				void *target_value = NULL;
				section_data *target_section = absolute_section;

				if (bfd_is_und_section(sym->section))
				{
					// Look up in linkset
					const symbol_data *found = search_symbol(linkset, sym->name);
					if (found != NULL)
					{
						target_value = found->value;
						target_section = found->section;
					}
					else if (sym->flags & BSF_WEAK)
						continue;
					else
					{
						fprintf(stderr, "Undefined symbol: %s\n", sym->name);
						free(relocs);
						free(bfd_symbols);
						return LOADER_ERROR_INVALID_FILE;
					}
				}
				else if (bfd_is_abs_section(sym->section))
				{
					target_value = (void *)sym->value;
					target_section = absolute_section;
				}
				else
				{
					// Local section symbol
					section_data key = {.name = (char *)sym->section->name};
					section_data *local_sec = sglib_section_data_find_member(assembly->sections, &key);
					if (local_sec != NULL)
					{
						if (local_sec->tls)
							target_value = (void *)((intptr_t)local_sec->address + sym->value);
						else
							target_value = (char *)local_sec->address + sym->value;
						target_section = local_sec;
					}
					else
					{
						target_value = (void *)sym->value;
					}
				}

				bfd_reloc_status_type status = aarch64_relocate(
					reloc->howto->type, abfd, sec, target_section, reloc->address, (bfd_vma)target_value, reloc->addend
				);

				if (status != bfd_reloc_ok && status != bfd_reloc_undefined)
				{
					fprintf(stderr, "Relocation failed for symbol %s: %d\n", sym->name, status);
					free(relocs);
					free(bfd_symbols);
					return LOADER_ERROR_INVALID_FILE;
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