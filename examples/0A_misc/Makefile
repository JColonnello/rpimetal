MODULE_DIR := examples/0A_misc
SOURCE_FILES := $(shell find "$(MODULE_DIR)" -name '*.c' -or -iname '*.s')
DIRS := $(shell find "$(MODULE_DIR)" -type d)
KERNEL_MODULES := arm/mmu-basic libc/libc libc/libgcc arm/irq drivers/timer sys/mux drivers/mbox drivers/uart

$(BUILD_DIR)/$(MODULE_DIR).ko: $(OBJ_FILES)
	$(CC) $^ -o $@ -nostdlib -Wl,-r
