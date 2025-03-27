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

typedef struct tls_data
{
	uint : 8;
	unsigned int thread_id;
	// unsigned int data_size;
	uint64_t data[];
} tls_data;

typedef struct tls_info
{
	unsigned int offset;
	unsigned int size;
	bool in_use;
	struct assembly_data *assembly;
	struct tls_info *prev;
} tls_info;

static assembly_data *loaded_assemblies;
static tls_data *tls_template;
static tls_info *tls_schema;

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

#pragma region Relocations

static void shift_and_apply_reloc(bfd *abfd, bfd_byte *data, reloc_howto_type *howto, bfd_vma relocation)
{
	relocation >>= (bfd_vma)howto->rightshift;
	/* Shift everything up to where it's going to be used.  */
	relocation <<= (bfd_vma)howto->bitpos;
	bfd_vma val = 0;
	switch (bfd_get_reloc_size(howto))
	{
	case 0:
		break;
	case 1:
		val = bfd_get_8(abfd, data);
		break;
	case 2:
		val = bfd_get_16(abfd, data);
		break;
	case 3:
		val = bfd_get_24(abfd, data);
		break;
	case 4:
		val = bfd_get_32(abfd, data);
		break;
#ifdef BFD64
	case 8:
		val = bfd_get_64(abfd, data);
		break;
#endif
	default:
		abort();
		break;
	}

	val = ((val & ~howto->dst_mask) | (((val & howto->src_mask) + relocation) & howto->dst_mask));

	switch (bfd_get_reloc_size(howto))
	{
	case 0:
		break;
	case 1:
		bfd_put_8(abfd, val, data);
		break;
	case 2:
		bfd_put_16(abfd, val, data);
		break;
	case 3:
		bfd_put_24(abfd, val, data);
		break;
	case 4:
		bfd_put_32(abfd, val, data);
		break;
#ifdef BFD64
	case 8:
		bfd_put_64(abfd, val, data);
		break;
#endif
	default:
		abort();
		break;
	}
}

static bfd_reloc_status_type
bfd_elf_adrp_hi_reloc (bfd *abfd,
		       arelent *reloc_entry,
		       asymbol *symbol,
		       void *data,
		       asection *input_section,
		       bfd *output_bfd,
		       char **error_message)
{
	reloc_howto_type *howto = reloc_entry->howto;
	bfd_size_type octets = reloc_entry->address * bfd_octets_per_byte (abfd, input_section);
  	if (!bfd_reloc_offset_in_range (howto, abfd, input_section, octets))
    	return bfd_reloc_outofrange;
	bfd_reloc_status_type flag = bfd_reloc_ok;

		
	asection *reloc_target_output_section;
	reloc_target_output_section = symbol->section->output_section;

  	/* Convert input-section-relative symbol value to absolute.  */
	bfd_vma output_base = 0;
	if ((output_bfd && ! howto->partial_inplace)
		|| reloc_target_output_section == NULL)
		output_base = 0;
	else
		output_base = reloc_target_output_section->vma;
	output_base += symbol->section->output_offset;

	if (bfd_get_flavour (abfd) == bfd_target_elf_flavour
      && (symbol->section->flags & SEC_ELF_OCTETS))
    	output_base *= bfd_octets_per_byte (abfd, input_section);

	bfd_vma relocation;
	if (bfd_is_com_section (symbol->section))
		relocation = 0;
	else
		relocation = symbol->value;
	relocation += output_base;

	/* Add in supplied addend.  */
	relocation += reloc_entry->addend;

	/* Here the variable relocation holds the final address of the
		symbol we are relocating against, plus any addend.  */

	relocation -= input_section->output_section->vma + input_section->output_offset;
	relocation = PG(relocation) - PG(reloc_entry->address);

	if (howto->complain_on_overflow != complain_overflow_dont
		&& flag == bfd_reloc_ok)
	  flag = bfd_check_overflow (howto->complain_on_overflow,
					 howto->bitsize,
					 howto->rightshift,
					 bfd_arch_bits_per_address (abfd),
					 relocation);

	static struct reloc_howto_struct hi_howto = 
	{
		.size = 4,
		.dst_mask = (1u<<24) - (1u<<5),
		.src_mask = 0,
		.rightshift = 14,
		.bitpos = 5,
	},
	lo_howto =
	{
		.size = 4,
		.dst_mask = (1u<<31) - (1u<<29),
		.src_mask = 0,
		.rightshift = 12,
		.bitpos = 29,
	};
	
	data = (bfd_byte *) data + octets;
	shift_and_apply_reloc(abfd, data, &hi_howto, relocation);
	shift_and_apply_reloc(abfd, data, &lo_howto, relocation);

	return flag;
}

const struct reloc_howto_struct adrp_howto = (struct reloc_howto_struct)
{
	.type = 275,
	.size = 4,
	.bitsize = 21,
	.rightshift = 12,
	.bitpos = 5,
	.complain_on_overflow = complain_overflow_signed,
	.pc_relative = true,
	.pcrel_offset = true,
	.special_function = bfd_elf_adrp_hi_reloc,
},
adrp_howto_nc = (struct reloc_howto_struct)
{
	.type = 275,
	.size = 4,
	.bitsize = 21,
	.rightshift = 12,
	.bitpos = 5,
	.complain_on_overflow = complain_overflow_dont,
	.pc_relative = true,
	.pcrel_offset = true,
	.special_function = bfd_elf_adrp_hi_reloc,
};;

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

	tls_info *tls_tmp_schema = tls_schema;
	size_t tls_tmp_template_size = tls_schema != NULL ? tls_schema->offset + tls_schema->size : 0;
	tls_tmp_template_size += sizeof(*tls_template);
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
			tls_info *tmpi = malloc(sizeof(tls_info));
			unsigned int offset = tls_tmp_schema != NULL ? tls_tmp_schema->offset + tls_tmp_schema->size : 0;
			unsigned int extra = offset % 1<<section->alignment_power;
			// Add empty space for alignment
			if(extra > 0)
			{
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
			*tmpi = (tls_info)
			{
				.in_use = true,
				.offset = offset,
				.size = section->size,
				.assembly = assembly,
			};
			SGLIB_LIST_ADD(tls_info, tls_tmp_schema, tmpi, prev);
			tls_template = realloc(tls_template, tls_tmp_template_size + extra + section->size);
			// Set pointer to copy section contents to template
			tls_tmp_template_size += extra;
			memory = &tls_template[tls_tmp_template_size];
			section->output_offset = tls_tmp_template_size;
			tls_tmp_template_size += section->size;
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
		char *tmp;
	
		for (int i = 0; i < relcount; i++)
		{
			arelent *reloc = relpp[i];
			asymbol *symbol = *reloc->sym_ptr_ptr;
			if(reloc->howto->type == 275)
				reloc->howto = &adrp_howto;
			else if(reloc->howto->type == 276)
				reloc->howto = &adrp_howto_nc;
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
