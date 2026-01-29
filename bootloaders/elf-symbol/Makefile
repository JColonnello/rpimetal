BOOT_MODULES := arm/mmu-basic libc/libc arm/irq drivers/timer sys/mux drivers/mbox drivers/uart complex-loader

$(BUILD_DIR)/kernel8.elf: $(DIR)/linker.ld $(OUTPUT_KO) \
		$(BUILD_DIR)/payloads/kernel_payload.o $(BUILD_DIR)/payloads/extra_payload.o \
		$(BOOT_MODULES:%=$(BUILD_DIR)/$(MODULES_DIR)/%.ko)
	$(CC) $(CFLAGS) -Wl,--unresolved-symbols=ignore-all -T $^ -o $@

$(BUILD_DIR)/payloads/kernel_payload.o: \
	$(BUILD_DIR)/extra/elf-init.ko \
	$(KERNEL_MODULES:%=$(BUILD_DIR)/$(MODULES_DIR)/%.ko) \
	$(BUILD_DIR)/$(KERNEL).ko

$(BUILD_DIR)/payloads/extra_payload.o: \
	$(EXTRA_MODULES:%=$(BUILD_DIR)/$(MODULES_DIR)/%.ko)

$(BUILD_DIR)/payloads/%.o $(BUILD_DIR)/payloads/%.c $(BUILD_DIR)/payloads/%.S &:
	@mkdir -p $(@D)
	@echo "Generating numbered payload source files..."
	sh bootloaders/elf-symbol/generate_payload.sh $(BUILD_DIR)/payloads "$*" $^
	$(CC) $(CFLAGS) $(INC_FLAGS) -r "$(BUILD_DIR)/payloads/$*.S" "$(BUILD_DIR)/payloads/$*.c" -o $@
