#pragma once
#define PACKAGE "elfloader-module"
#define PACKAGE_VERSION "0.1"

#include <bfd.h>

#ifndef LOADER_SYMBOL_BIND
#define LOADER_SYMBOL_BIND
enum symbol_bind
{
	SYMBOL_BIND_GLOBAL,
	SYMBOL_BIND_LOCAL,
	SYMBOL_BIND_WEAK,
};
#endif

typedef struct section_data
{
	const char *name;
	void *address;
	size_t size;
	bool tls;
	asection *bfd_section;
	struct section_data *next;
} section_data;

typedef struct symbol_data
{
	const char *name;
	void *address;
	enum symbol_bind type;
	struct section_data *section;
} symbol_data;

typedef struct assembly_data
{
	const char *name;
	struct section_data *sections;
	uint16_t symbol_count;
	struct symbol_data *symbols;
	bfd *bfd_file;

	struct assembly_data *next;
} assembly_data;

typedef struct tructor_data
{
	uint16_t priority;
	struct section_data *section;
	struct tructor_data *next;
} tructor_data;

typedef struct tls_info
{
	unsigned int offset;
	unsigned int size;
	bool in_use;
	struct section_data *section;
	struct tls_info *next;
} tls_info;
