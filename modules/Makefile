$(BUILD_DIR)/payload.o: $(BUILD_DIR)/$(MODULES_DIR)/test.ko
	$(ARMGNU)-objcopy --input binary \
    --output elf64-littleaarch64 \
    --rename-section .data=.text,CONTENTS,ALLOC,LOAD,READONLY,DATA \
    $< $@

# $(MODULES_DIR)/main: main.c.o payload.o
# 	$(CC) $(CFLAGS) $^ -o $@ $(LIBS) 
	
	
# gcc -c test_unit.c -T simple_obj.ld -o test_unit.o -nostdlib -nostartfiles -nodefaultlibs	
# objcopy --remove-section .eh_frame --remove-section .comment --remove-section .note.GNU-stack test_unit.o test_unit.o