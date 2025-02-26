#define PACKAGE "circe-dynload-test"
#define PACKAGE_VERSION "0.1"
#define _GNU_SOURCE

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

typedef struct section_data
{
	const char *name;
	void *address;

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
		SGLIB_ARRAY_BINARY_SEARCH(symbol_data, assembly->symbols, 0, assembly->symbol_count, name, SEARCH_FUNC, found, index);
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

unsigned int loader_init()
{
	assembly_data *start = calloc(1, sizeof(assembly_data));
	sglib_assembly_data_add(&loaded_assemblies, start);

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

	section_data *loaded_sections = NULL;
	// first pass through section table to allocate memory and set output offsets
	for (asection *section = abfd->sections; section != NULL; section = section->next)
	{
		flagword flags = section->flags;
		// skip section if not meant to be loaded
		if (!(flags & SEC_LOAD))
			continue;
		
		// void *memory = aligned_alloc(1<<section->alignment_power, section->size);
		void *memory = aligned_alloc(0x1000, section->size);
		bfd_get_section_contents(abfd, section, memory, 0, section->size);
		section->output_offset = (bfd_vma)memory;
		if(strcmp(section->name, ".text") == 0)
			printf("add-symbol-file %s 0x%08lx", filename, section->output_offset);
		else
			printf(" -s %s 0x%08lx", section->name, section->output_offset);

		section_data *sec = malloc(sizeof(section_data));
		*sec = (section_data)
		{
			.name = strdup(section->name),
			.address = memory,
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
			symbol->value += symbol->section->output_offset;
			symbols_simple[symbols_simple_count++] = (symbol_data)
			{
				.name = strdup(symbol->name),
				.address = (void*)symbol->value,
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
		char *tmp;
	
		for (int i = 0; i < relcount; i++)
		{
			arelent *reloc = relpp[i];
			asymbol *symbol = *reloc->sym_ptr_ptr;
			
			if(!bfd_is_und_section(symbol->section) || symbol->value != 0)
			{
				section->output_section = symbol->section;
				printf("Relocating symbol (%s) to section (%s) at 0x%08lx + 0x%08lx = 0x%08lx\n", symbol->name, section->output_section->name, section->output_section->output_offset, symbol->value, section->output_section->output_offset + symbol->value);
				bfd_reloc_status_type status = bfd_perform_relocation(abfd, reloc, (void*)section->output_offset, section, NULL, &tmp);
				if(status == bfd_reloc_dangerous)
					printf("Dangerous relocation: %s\n", tmp);
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

	assembly_data *assembly = malloc(sizeof(assembly_data));
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
