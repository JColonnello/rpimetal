MODULE_DIR := modules/kernel
SOURCE_FILES := $(shell find "$(MODULE_DIR)" -name '*.c' -or -iname '*.s' | tr '\n' ' ')
OBJ_FILES := $(SOURCE_FILES:%=$(BUILD_DIR)/%.o)
DIRS := $(shell find "$(MODULE_DIR)" -type d | tr '\n' ' ')
LINKED_MODULES := arm/mmu-basic libc/libc libc/libgcc arm/irq drivers/mbox drivers/uart drivers/display drivers/timer

$(BUILD_DIR)/$(MODULE_DIR).ko: $(OBJ_FILES) $(LINKED_MODULES:%=$(BUILD_DIR)/$(MODULES_DIR)/%.ko)
	$(CC) $(filter-out %.mk,$^) -o $@ -nostdlib -Wl,-r

$(BUILD_DIR)/$(MODULE_DIR).mk: $(DIRS)

-include $(OBJ_FILES:%.o=%.d)
