#define PACKAGE "elfloader-module"
#define PACKAGE_VERSION "0.1"

#include <stddef.h>
#include <stdint.h>
#include <sys/_intsup.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <bfd.h>
#include <fcntl.h>
#include <sglib.h>
#include "loader.h"

#define PG(x)	     ((x) & ~ (bfd_vma) 0xfff)
#define PG_OFFSET(x) ((x) &   (bfd_vma) 0xfff)

#pragma region Type definitions

typedef struct section_data
{
	const char *name;
	void *address;
	bool tls;
	struct section_data *next;
} section_data;

typedef struct assembly_data
{
	const char *name;
	struct section_data *sections;
	uint16_t symbol_count;
	struct symbol_data *symbols;

	struct assembly_data *next;
} assembly_data;

static assembly_data *loaded_assemblies;

#define NAME_COMP(x,y) ((x)->name == NULL || (y)->name == NULL ? SGLIB_SAFE_NUMERIC_COMPARATOR((x)->name, (y)->name) : strcmp((x)->name, (y)->name))
#define COMP_DIRECT(comp,x,y) comp(&x,&y)
#define NAME_COMP_DIRECT(x,y) COMP_DIRECT(NAME_COMP, x, y)

SGLIB_DEFINE_LIST_PROTOTYPES(assembly_data, NAME_COMP, next)
SGLIB_DEFINE_LIST_FUNCTIONS(assembly_data, NAME_COMP, next)
SGLIB_DEFINE_LIST_PROTOTYPES(section_data, NAME_COMP, next)
SGLIB_DEFINE_LIST_FUNCTIONS(section_data, NAME_COMP, next)

#pragma endregion

static void free_assembly_data(assembly_data *assembly)
{
	free((void*)assembly->name);
	
	for (int i = 0; i < assembly->symbol_count; i++)
		free((void*)assembly->symbols[i].name);
	free(assembly->symbols);
	
	struct sglib_section_data_iterator it;
	for(section_data *s = sglib_section_data_it_init(&it, assembly->sections); s != NULL; s = sglib_section_data_it_next(&it))
	{
		free((void*)s->name);
		if(!s->tls)
			free(s->address);
		free(s);
	}

	free(assembly);
}

static const symbol_data *search_symbol(const char *name)
{
	#define SEARCH_FUNC(p, k) strcmp((&p)->name, k)

	struct sglib_assembly_data_iterator it;
	bool found = false;
	int index = -1;
	for(assembly_data *assembly = sglib_assembly_data_it_init(&it, loaded_assemblies); assembly != NULL; assembly = sglib_assembly_data_it_next(&it))
	{
		SGLIB_ARRAY_BINARY_SEARCH(symbol_data, assembly->symbols, 0, assembly->symbol_count - 1, name, SEARCH_FUNC, found, index);
		if(found)
			return &assembly->symbols[index];
	}
	return NULL;

	#undef SEARCH_FUNC
}

void *loader_search_symbol(const char *name)
{
	const symbol_data *sym = search_symbol(name);
	if(sym != NULL)
		return sym->address;
	else
		return NULL;
}

// Returns the first number greater or equal than "value" which is a multiple of "size"
// static size_t _round_up_to_multiple_of(size_t value, size_t size)
// {
// 	size_t r = value % size;
// 	return r == 0 ? value : value - r + size;
// }
static void loader_initial_tls(const void * restrict data, size_t size, size_t alignment);

unsigned int loader_init()
{
	extern const void _tcb, __tbss_end__;
	assembly_data *start = calloc(1, sizeof(assembly_data));
	sglib_assembly_data_add(&loaded_assemblies, start);
    loader_initial_tls(&_tcb + 0x10, &__tbss_end__ - &_tcb - 0x10, 0x10);
    struct tls_data *tcb = loader_create_tcb();

    loader_switch_tcb(tcb);

	// init libbfd
	return bfd_init();
}

void loader_destroy()
{
	free_assembly_data(loaded_assemblies);
}

void loader_add_starting_symbols(size_t n, const symbol_data symbols[static n])
{
	assembly_data *match, key = { .name = NULL };
	match = sglib_assembly_data_find_member(loaded_assemblies, &key);
	size_t count = match->symbol_count;
	match->symbols = reallocarray(match->symbols, count + n, sizeof(symbol_data));
	memcpy(&match->symbols[count], symbols, sizeof(symbol_data) * n);
	match->symbol_count = count + n;
}

#pragma region TLS

typedef struct __attribute__((packed)) tls_data
{
	uint64_t _res1;
	uint32_t thread_id;
	uint32_t _res2;
	// unsigned int data_size;
	uint64_t data[];
} tls_data;

typedef struct tls_info
{
	unsigned int offset;
	unsigned int size;
	bool in_use;
	const struct assembly_data *assembly;
	struct tls_info *prev;
} tls_info;

static tls_data *tls_template;
tls_info *tls_schema;

static ssize_t register_tls_segment(size_t size, size_t alignment, const assembly_data *assembly)
{
	tls_info *tls_tmp_schema = tls_schema;
	size_t tls_tmp_template_size = tls_schema != NULL ? tls_schema->offset + tls_schema->size : 0;

	tls_info *tmpi;
	ssize_t offset = tls_tmp_template_size;
	ssize_t extra = offset % alignment;
	// Add empty space for alignment
	if(extra > 0)
	{
		tmpi = malloc(sizeof(tls_info));
		*tmpi = (tls_info)
		{
			.in_use = false,
			.offset = offset,
			.size = extra,
		};
		SGLIB_LIST_ADD(tls_info, tls_tmp_schema, tmpi, prev);
		offset += tmpi->size;
	}
	// Add to TLS schema
	tmpi = malloc(sizeof(tls_info));
	*tmpi = (tls_info)
	{
		.in_use = true,
		.offset = offset,
		.size = size,
		.assembly = assembly,
	};
	SGLIB_LIST_ADD(tls_info, tls_tmp_schema, tmpi, prev);
	tls_template = realloc(tls_template, sizeof(*tls_template) + tls_tmp_template_size + extra + size);
	tls_tmp_template_size += extra + size;
	tls_schema = tls_tmp_schema;
	return offset;
}

void *loader_tls_ptr(const tls_data *tcb, ssize_t offset)
{
	return (void*)tcb + sizeof(*tcb) + offset;
}

static void loader_initial_tls(const void * restrict data, size_t size, size_t alignment)
{
	ssize_t offset = register_tls_segment(size, alignment, loaded_assemblies);
	void * restrict ptr = loader_tls_ptr(tls_template, offset);
	memcpy(ptr, data, size);
}

void *local_tls_offset(void *var);

tls_data *loader_create_tcb()
{
	if(tls_schema == NULL)
		return NULL;
	size_t size = tls_schema->offset + tls_schema->size;
	tls_data *tcb = malloc(size + sizeof(tls_data));
	memcpy(tcb, tls_template, size);
	return tcb;
}

struct tls_data *loader_switch_tcb(struct tls_data *tcb)
{
	struct tls_data *tmp = __builtin_thread_pointer();
	if(tcb != NULL)
	{
		asm ("msr tpidr_el1, %0"
			: 
			: "r" (tcb));
	}
	return tmp;
}

void loader_print_tls_layout(const struct tls_info *schema)
{
	printf("TLS Schema:\n");
	const assembly_data *last = NULL;
	SGLIB_LIST_MAP_ON_ELEMENTS(const tls_info, schema, segment, prev, {
		if(segment->in_use)
		{
			if(last != segment->assembly)
			{
				last = segment->assembly;
				printf("Assembly: %s\n", last->name != NULL ? last->name : "LOCAL");
			}
			printf("\tOffset = %u, Size = %u\n", segment->offset, segment->size);
		}
		else
			printf("\tAlignment = %u\n", segment->size);
	});
}

#pragma endregion

#pragma region Relocations

extern bfd_vma _bfd_aarch64_elf_resolve_relocation(bfd *input_bfd, bfd_reloc_code_real_type r_type, bfd_vma place,
                                                   bfd_vma value, bfd_vma addend, bool weak_undef_p);

extern bfd_reloc_status_type _bfd_aarch64_elf_put_addend(bfd *abfd, bfd_byte *address, bfd_reloc_code_real_type r_type,
                                                         reloc_howto_type *howto, bfd_signed_vma addend);
extern bfd_reloc_code_real_type elf64_aarch64_bfd_reloc_from_type(bfd *abfd, unsigned int r_type);
extern reloc_howto_type *elf64_aarch64_howto_from_type(bfd *abfd, unsigned int r_type);

bfd_reloc_status_type aarch64_relocate(unsigned int r_type, bfd *input_bfd, asection *input_section, bfd_vma offset,
                                       bfd_vma value, bfd_vma addend)
{
	reloc_howto_type *howto;
	bfd_vma place;

	howto = elf64_aarch64_howto_from_type(input_bfd, r_type);
	place = (input_section->output_section->vma + input_section->output_offset + offset);

	r_type = elf64_aarch64_bfd_reloc_from_type(input_bfd, r_type);
	value += input_section->output_section->output_offset;
	value = _bfd_aarch64_elf_resolve_relocation(input_bfd, r_type, place, value, addend, false);
	return _bfd_aarch64_elf_put_addend(input_bfd, (bfd_byte *)place, r_type, howto, value);
}


#pragma endregion

int loader_load_file(FILE *file, const char *filename)
{
	// load ELF file
	bfd *abfd = bfd_openstreamr(filename, NULL, file);

	// no section info is loaded unless we call bfd_check_format!:
	if (!bfd_check_format(abfd, bfd_object))
	{
		printf("Failed to open object file!\n");
		exit(-1);
	}

	assembly_data *assembly = malloc(sizeof(assembly_data));

	section_data *loaded_sections = NULL;
	// first pass through section table to allocate memory and set output offsets
	for (asection *section = abfd->sections; section != NULL; section = section->next)
	{
		flagword flags = section->flags;
		// skip section if not meant to be loaded
		if (!(flags & SEC_ALLOC))
			continue;
		
		void *memory;
		// If it's thread local data, add to TLS template
		if (flags & SEC_THREAD_LOCAL)
		{
			ssize_t offset = register_tls_segment(section->size, 1<<section->alignment_power, assembly);
			// Set pointer to copy section contents to template
			memory = loader_tls_ptr(tls_template, offset);
			section->output_offset = offset;
		}
		else
		{
			memory = aligned_alloc(0x1000, section->size);
			section->output_offset = (bfd_vma)memory;
		}

		// void *memory = aligned_alloc(1<<section->alignment_power, section->size);
		// Load from file or zero out depending on flag
		if(flags & SEC_LOAD)
			bfd_get_section_contents(abfd, section, memory, 0, section->size);
		else
			memset(memory, 0, section->size);

		if(strcmp(section->name, ".text") == 0)
			printf("add-symbol-file %s 0x%08lx", filename, section->output_offset);
		else
			printf(" -s %s 0x%08lx", section->name, section->output_offset);

		section_data *sec = malloc(sizeof(section_data));
		*sec = (section_data)
		{
			.name = strdup(section->name),
			.tls = !!(flags & SEC_THREAD_LOCAL),
			.address = (void*)section->output_offset,
		};
		sglib_section_data_add(&loaded_sections, sec);
	}

	printf("\n");
	sglib_section_data_reverse(&loaded_sections);
	
	// load the symbol table from the object file
	size_t symsize = bfd_get_symtab_upper_bound(abfd);
	asymbol **symbols = malloc(symsize);
	int symcount = bfd_canonicalize_symtab(abfd, symbols);
	symbol_data *symbols_simple = malloc(sizeof(symbol_data) * symcount);
	size_t symbols_simple_count = 0;
	for (int i = 0; i < symcount; i++)
	{
		asymbol *symbol = symbols[i];
		// Symbols in undefined section have to be pointed to external symbols
		if(bfd_is_und_section(symbol->section))
		{
			const symbol_data *ref = search_symbol(symbol->name);
			if(ref != NULL)
				symbol->value = (symvalue)ref->address;
			else
				fprintf(stderr, "Can't find undefined symbol: %s\n", symbol->name);

		}
		else if(symbol->flags & BSF_GLOBAL)
		{
			symbols_simple[symbols_simple_count++] = (symbol_data)
			{
				.name = strdup(symbol->name),
				.address = symbol->section->output_offset + (void*)symbol->value,
			};
		}
	}
	
	struct sglib_section_data_iterator sd_it;
	section_data *sd_cur = sglib_section_data_it_init(&sd_it, loaded_sections);
	for (asection *section = abfd->sections; section != NULL; section = section->next)
	{
		if(sd_cur == NULL)
			break;
		else if(NAME_COMP(sd_cur, section) == 0)
			sd_cur = sglib_section_data_it_next(&sd_it);
		else
			continue;

		// Now we load the relocation table
		long relsize = bfd_get_reloc_upper_bound(abfd, section);
		arelent **relpp = malloc(relsize);
		long relcount = bfd_canonicalize_reloc(abfd, section, relpp, symbols);
	
		for (int i = 0; i < relcount; i++)
		{
			arelent *reloc = relpp[i];
			asymbol *symbol = *reloc->sym_ptr_ptr;
			if(!bfd_is_und_section(symbol->section) || symbol->value != 0)
			{
				section->output_section = symbol->section;
				printf("Relocating symbol (%s) to section (%s) at 0x%08lx + 0x%08lx = 0x%08lx\n", symbol->name, section->output_section->name, section->output_section->output_offset, symbol->value, section->output_section->output_offset + symbol->value);
				bfd_reloc_status_type status = aarch64_relocate(reloc->howto->type, abfd, section, reloc->address, symbol->value, reloc->addend);
				if(status == bfd_reloc_dangerous)
					printf("Dangerous relocation\n");
				else if(status != bfd_reloc_ok && status != bfd_reloc_undefined)
					printf("Failed relocation. Error %d\n", status);
			}
			else
				printf("Symbol (%s) is undefined\n", symbol->name);
		}
		free(relpp);
	}
	free(symbols);
	symbols_simple = reallocarray(symbols_simple, sizeof(symbol_data), symbols_simple_count);
	SGLIB_ARRAY_SINGLE_QUICK_SORT(symbol_data, symbols_simple, symbols_simple_count, NAME_COMP_DIRECT);

	*assembly = (assembly_data)
	{
		.name = bfd_get_filename(abfd),
		.sections = loaded_sections,
		.symbol_count = symbols_simple_count,
		.symbols = symbols_simple,
	};
	sglib_assembly_data_add(&loaded_assemblies, assembly);

	bfd_close(abfd);
	printf("Finished loading %s\n", filename);
	return 0;
}
