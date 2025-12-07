# Bootloader binary
DIR = bootloaders/elf-symbol

SOURCE_FILES := $(shell find $(DIR) -iname '*.c' -or -iname '*.s')
BOOT_MODULES := arm/mmu-basic libc/libc arm/irq drivers/timer sys/mux drivers/mbox drivers/uart loader

$(BUILD_DIR)/kernel8.elf: $(DIR)/linker.ld $(OBJ_FILES) $(BUILD_DIR)/payload.o $(BOOT_MODULES:%=$(BUILD_DIR)/$(MODULES_DIR)/%.ko)
	$(CC) $(CFLAGS) -Wl,--unresolved-symbols=ignore-all -o $@ -T $^
#	$(LD) $(LDFLAGS) -T $(BOOT_SRC_DIR)/linker.ld -o $(BUILD_DIR)/kernel8.elf $(OBJ_FILES) -l:crti.o -l:crtbegin.o -l:crt0.o -lbfd -lz -liberty -lc -lgcc -lsframe -l:crtend.o -l:crtn.o $(BUILD_DIR)/modules/payload.o

$(BUILD_DIR)/payload.o: $(MODULES:%=$(BUILD_DIR)/$(MODULES_DIR)/%.ko) $(BUILD_DIR)/kernel.ko
	$(LD) -r -b binary -o $@ $^