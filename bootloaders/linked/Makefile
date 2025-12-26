BOOT_MODULES := arm/mmu-basic
BOOT_MODULES := $(filter-out $(KERNEL_MODULES), $(BOOT_MODULES))

$(BUILD_DIR)/kernel8.elf : $(OUTPUT_DIR).elf
	ln -f $< $@

$(OUTPUT_DIR).elf: $(DIR)/linker.ld $(OBJ_FILES) $(BUILD_DIR)/kernel.ko $(BOOT_MODULES:%=$(BUILD_DIR)/$(MODULES_DIR)/%.ko)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -nostdlib -Wl,--unresolved-symbols=ignore-all -T $^ -o $@

$(BUILD_DIR)/kernel.ko: $(BUILD_DIR)/extra/elf-init.ko $(KERNEL_MODULES:%=$(BUILD_DIR)/$(MODULES_DIR)/%.ko) $(BUILD_DIR)/$(KERNEL).ko
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -nostdlib -r $^ -o $@
