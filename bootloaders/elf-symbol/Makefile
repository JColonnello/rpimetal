DIR = bootloaders/elf-symbol

SOURCE_FILES := $(shell find $(DIR) -iname '*.c' -or -iname '*.s')
BOOT_MODULES := arm/mmu-basic libc/libc arm/irq drivers/timer sys/mux drivers/mbox drivers/uart loader

$(BUILD_DIR)/kernel8.elf: $(DIR)/linker.ld $(OBJ_FILES) $(BUILD_DIR)/payload.o $(BOOT_MODULES:%=$(BUILD_DIR)/$(MODULES_DIR)/%.ko)
	$(CC) $(CFLAGS) -Wl,--unresolved-symbols=ignore-all -o $@ -T $^

$(BUILD_DIR)/payload.o: $(EXTRA_MODULES:%=$(BUILD_DIR)/$(MODULES_DIR)/%.ko) $(BUILD_DIR)/kernel.ko
	$(LD) -r -b binary -o $@ $^

$(BUILD_DIR)/kernel.ko: $(BUILD_DIR)/$(KERNEL).ko $(KERNEL_MODULES:%=$(BUILD_DIR)/$(MODULES_DIR)/%.ko)
	$(CC) -r -o $@ $^
