BOOT_MODULES := arm/mmu-basic libc/libc arm/irq drivers/timer sys/mux drivers/mbox drivers/uart loader drivers/sd2 fs/fat

$(BUILD_DIR)/kernel8.elf: $(DIR)/linker.ld $(OUTPUT_KO) $(BUILD_DIR)/payload.o $(BOOT_MODULES:%=$(BUILD_DIR)/$(MODULES_DIR)/%.ko)
	$(CC) $(CFLAGS) -Wl,--unresolved-symbols=ignore-all -T $^ -o $@

$(BUILD_DIR)/payload.o: $(EXTRA_MODULES:%=$(BUILD_DIR)/$(MODULES_DIR)/%.ko) $(BUILD_DIR)/kernel.ko
	$(LD) -r -b binary $^ -o $@

$(BUILD_DIR)/kernel.ko: $(BUILD_DIR)/extra/elf-init.ko $(KERNEL_MODULES:%=$(BUILD_DIR)/$(MODULES_DIR)/%.ko) $(BUILD_DIR)/$(KERNEL).ko
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -nostdlib -r $^ -o $@
