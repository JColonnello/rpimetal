MODULE_DIR := examples/0A_misc
SOURCE_FILES := $(shell find "$(MODULE_DIR)" -name '*.c' -or -iname '*.s')
OBJ_FILES := $(SOURCE_FILES:%=$(BUILD_DIR)/%.o)
DIRS := $(shell find "$(MODULE_DIR)" -type d)
LINKED_MODULES := arm/mmu-basic libc/libc libc/libgcc arm/irq drivers/timer sys/mux drivers/mbox drivers/uart

$(BUILD_DIR)/$(MODULE_DIR).ko: $(OBJ_FILES) $(LINKED_MODULES:%=$(BUILD_DIR)/$(MODULES_DIR)/%.ko)
	$(CC) $(filter-out %.mk,$^) -o $@ -nostdlib -Wl,-r

$(BUILD_DIR)/kernel.ko: $(BUILD_DIR)/$(MODULE_DIR).ko
	ln -f $< $@

-include $(OBJ_FILES:%.o=%.d)
