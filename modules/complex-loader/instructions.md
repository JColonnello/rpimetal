Based on the single pass ELF loader found in #file:modules/loader/loader.c, implement a multiple pass linker/elf loader. 

It will run multiple passes on the input assemblies:

1. In the first pass, it should 