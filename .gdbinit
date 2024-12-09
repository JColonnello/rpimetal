file build/kernel8.elf
break undef_func
break *0x200
target remote :1234
tui layout split
tui focus cmd