#pragma once
#define PACKAGE "elfloader-module"
#define PACKAGE_VERSION "0.1"

#include "_loader.h"
#include <bfd.h>

bfd_reloc_status_type aarch64_relocate(
	unsigned int r_type,
	bfd *input_bfd,
	section_data *input_section,
	section_data *output_section,
	bfd_vma offset,
	bfd_vma value,
	bfd_vma addend
);