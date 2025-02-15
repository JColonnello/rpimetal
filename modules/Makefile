TARGET = $(MODULES_DIR)/payload.o
MODULES_DIR = build/modules
# LIBS = -l:libiberty.a -l:libz.a -l:libbfd.a

.PHONY: default all clean

default: $(TARGET)
all: default

# %.o: %.c
# 	$(CC) $(CFLAGS) -c $< -o $@

$(MODULES_DIR)/test_unit.ko: $(MODULES_DIR)/test_unit.c.o
	$(CC) -r -static-libgcc -Wl,-Bstatic -lc $(CFLAGS) $^ -o $@

$(MODULES_DIR)/payload.o: $(MODULES_DIR)/test_unit.ko
	$(ARMGNU)-objcopy --input binary \
    --output elf64-littleaarch64 \
    --rename-section .data=.text,CONTENTS,ALLOC,LOAD,READONLY,DATA \
    $< $@

# $(MODULES_DIR)/main: main.c.o payload.o
# 	$(CC) $(CFLAGS) $^ -o $@ $(LIBS) 
	
	
# gcc -c test_unit.c -T simple_obj.ld -o test_unit.o -nostdlib -nostartfiles -nodefaultlibs	
# objcopy --remove-section .eh_frame --remove-section .comment --remove-section .note.GNU-stack test_unit.o test_unit.o