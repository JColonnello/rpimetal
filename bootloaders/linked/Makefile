BOOT_MODULES := arm/mmu-basic

$(BUILD_DIR)/kernel8.elf : $(OUTPUT_DIR).elf
	ln -f $< $@

$(OUTPUT_DIR).elf: $(DIR)/linker.ld $(OBJ_FILES) $(BUILD_DIR)/kernel.ko $(BOOT_MODULES:%=$(BUILD_DIR)/$(MODULES_DIR)/%.ko)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -nostdlib -T $^ -o $@

$(BUILD_DIR)/kernel.ko: $(BUILD_DIR)/$(KERNEL).ko $(KERNEL_MODULES:%=$(BUILD_DIR)/$(MODULES_DIR)/%.ko)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -Wl,-r $^ -o $@
