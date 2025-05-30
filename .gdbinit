source ./gdb-commands.py
break src/bootloader/boot.c:71
commands
silent
add-assembly-symbols
cont
end