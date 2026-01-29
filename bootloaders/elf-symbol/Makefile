BOOT_MODULES := arm/mmu-basic libc/libc arm/irq drivers/timer sys/mux drivers/mbox drivers/uart complex-loader

$(BUILD_DIR)/kernel8.elf: $(DIR)/linker.ld $(OUTPUT_KO) $(BUILD_DIR)/payload.o $(BOOT_MODULES:%=$(BUILD_DIR)/$(MODULES_DIR)/%.ko)
	$(CC) $(CFLAGS) -Wl,--unresolved-symbols=ignore-all -T $^ -o $@

$(BUILD_DIR)/payload.o: $(EXTRA_MODULES:%=$(BUILD_DIR)/$(MODULES_DIR)/%.ko) $(BUILD_DIR)/kernel.ko
	@mkdir -p $(BUILD_DIR)
	@echo "Generating numbered payload source files..."
	@sh bootloaders/elf-symbol/generate_payload.sh $(BUILD_DIR)/payload.S $(BUILD_DIR)/payload.c $^
# Compile/assemble and produce a relocatable payload.o
	$(CC) -c -o $(BUILD_DIR)/payload.S.o $(BUILD_DIR)/payload.S $(CFLAGS)
	$(CC) -c -o $(BUILD_DIR)/payload.c.o $(BUILD_DIR)/payload.c $(CFLAGS)
	$(LD) -r -o $@ $(BUILD_DIR)/payload.S.o $(BUILD_DIR)/payload.c.o

$(BUILD_DIR)/kernel.ko: $(BUILD_DIR)/extra/elf-init.ko $(KERNEL_MODULES:%=$(BUILD_DIR)/$(MODULES_DIR)/%.ko) $(BUILD_DIR)/$(KERNEL).ko
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -nostdlib -r $^ -o $@
