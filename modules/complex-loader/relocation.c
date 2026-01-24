#include "relocation.h"
#include "_loader.h"

extern bfd_vma _bfd_aarch64_elf_resolve_relocation(
	bfd *input_bfd, bfd_reloc_code_real_type r_type, bfd_vma place, bfd_vma value, bfd_vma addend, bool weak_undef_p
);

extern bfd_reloc_status_type _bfd_aarch64_elf_put_addend(
	bfd *abfd, bfd_byte *address, bfd_reloc_code_real_type r_type, reloc_howto_type *howto, bfd_signed_vma addend
);
extern bfd_reloc_code_real_type elf64_aarch64_bfd_reloc_from_type(bfd *abfd, unsigned int r_type);
extern reloc_howto_type *elf64_aarch64_howto_from_type(bfd *abfd, unsigned int r_type);

bfd_reloc_status_type aarch64_relocate(
	unsigned int r_type,
	bfd *input_bfd,
	section_data *input_section,
	section_data *output_section,
	bfd_vma offset,
	bfd_vma value,
	bfd_vma addend
)
{
	reloc_howto_type *howto;
	bfd_vma place;

	howto = elf64_aarch64_howto_from_type(input_bfd, r_type);
	place = (bfd_vma)input_section->address + offset;

	r_type = elf64_aarch64_bfd_reloc_from_type(input_bfd, r_type);
	value += (bfd_vma)output_section->address;
	value = _bfd_aarch64_elf_resolve_relocation(input_bfd, r_type, place, value, addend, false);
	// R_AARCH64_ABS64, R_AARCH64_ABS32, R_AARCH64_ABS16
	if (howto->type >= 257 && howto->type <= 259)
		value += addend;
	return _bfd_aarch64_elf_put_addend(input_bfd, (bfd_byte *)place, r_type, howto, value);
}