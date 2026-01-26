Based on the single pass ELF loader found in modules/loader/loader.c, implement a multiple pass linker/elf loader and its public interface found in include/complex-loader.h. 

It must allow the user to load some files through `loader_read_file`, and when `loader_finish_link` is called, it will link these files between themselves and to previously loaded files. Meaning, files loaded in one batch (by succesive `loader_read_file` calls) can reference each other and previously loaded files, but not files loaded after `loader_finish_link` is called ('closing' the batch). It will run multiple steps on the input assemblies to achive the final linked output

On `loader_read_file`: 
1. Read the input ELF files and collect all its sections and symbol definitions. It will store this information in the structures contained inside the `link_set` structure passed to it.

On `loader_finish_link`:
1. Go through all the collected sections and allocate memory for them as needed. For this, it should use a dedicated allocator, a triplet of functions `section_malloc`, `section_free` and `section_realloc` (for now, they wrap `malloc` and `free`). TLS sections from the same batch should be allocated contiguously in a TLS area (by using `section_malloc` anyway), preceeded by the TCB, and forming the TLS template. The alignment requirements of each section must be respected, so it may be necessary to leave empty spaces between them.

	**Important!**: ALL TLS sections must be contiguous in memory, as they are addressed relative to the thread pointer. If two batches contain TLS sections, the TLS template should be reallocated to contain both batches' TLS sections, with the first batch's sections first, followed by the second batch's sections.

	Each TLS section will have a corresponding entry in the TLS schema structure `tls_info`. The offset of each TLS section within that area should be stored in the corresponding `section_data` structure (instead of the absolute address in memory, like normal sections). Regular sections can be allocated independently. After allocation, copy the contents of each section from the ELF file to the allocated memory.

2. Read the relocation entries from each section and apply them, resolving symbol references by looking up the symbols in the collected symbol tables in the `link_set`. Take into account binding types, preemption rules, and TLS sections:
   - For local symbols, resolve them within the same assembly.
   - For global symbols, look them up from the beginning of the `link_set`, stopping at the first match.
   - For weak symbols, use the last defined value if no strong definition is found, or the first global definition, or NULL if none exists.
   - For relocations found in TLS sections, take into account that the section address is relative from the start of the TLS template, so add the TLS template address to the relocation address.
3. After all relocations are applied, mark the assemblies as loaded, close the bfd instances of each assembly and set it to NULL, and set the `asection` pointer in `section_data` to NULL.

The code must be implemented *VERY* modularly for maintainability and extensibility, taking the working loader module as a reference. Comment the code properly for clarity.