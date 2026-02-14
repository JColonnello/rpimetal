
BOOT_MODULES := arm/mmu-basic libc/libc arm/irq drivers/timer sys/mux drivers/mbox drivers/uart complex-loader

$(BUILD_DIR)/kernel8.elf: $(DIR)/linker.ld $(OUTPUT_KO) $(BOOT_MODULES:%=$(BUILD_DIR)/$(MODULES_DIR)/%.ko) $(BUILD_DIR)/file-list.txt
	$(CC) $(CFLAGS) -Wl,--unresolved-symbols=ignore-all -T $^ -o $@

# Generate a file-list for upload-tool. The list contains kernel-phase files,
# a blank separator line, then extra-phase files. Paths match the payload
# generator output so GDB can map symbols by path.
$(BUILD_DIR)/file-list.txt:
	@mkdir -p $(@D)
	@echo "Generating file list for elf-uart..."
	@{ \
	  echo $(BUILD_DIR)/extra/elf-init.ko; \
	  for m in $(KERNEL_MODULES); do echo $(BUILD_DIR)/$(MODULES_DIR)/$$m.ko; done; \
	  echo $(BUILD_DIR)/$(KERNEL).ko; \
	  echo ""; \
	  for m in $(EXTRA_MODULES); do echo $(BUILD_DIR)/$(MODULES_DIR)/$$m.ko; done; \
	} > $@

