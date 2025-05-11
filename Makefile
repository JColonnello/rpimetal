include Makefile.inc

BUILD_DIR = build
MODULES_DIR = modules
SRC_DIR = src/bootloader
IMAGE = kernel8.img
KERNEL = kernel
BOOT_MODULES = drivers/uart loader arm/irq drivers/mbox
MODULES = testing/test
MODULES += $(KERNEL)

# Phony targets

.PHONY: all clean rebuild run debug uart0 toolchain

all: $(IMAGE)

clean:
	rm -rf $(BUILD_DIR) *.img

rebuild: clean all

run: all
	qemu-system-aarch64 -M raspi3b -kernel kernel8.img -serial tcp:localhost:4444 -d int -vnc :1,websocket=on

debug: all
	qemu-system-aarch64 -M raspi3b -kernel kernel8.img -serial tcp:localhost:4444 -nographic -d int -vnc :1,websocket=on -S -s

uart0:
	nc -lkvp 4444

toolchain: toolchain/Dockerfile
	docker build -t rpimetal-toolchain toolchain/

# Empty recipes

%.d: ;

# Module file and recipe

include $(MODULES_DIR)/Makefile

$(STD_MODULES:%=$(BUILD_DIR)/$(MODULES_DIR)/%.ko): %.ko: %.mk

$(BUILD_DIR)/$(MODULES_DIR)/%.mk: $(MODULES_DIR)/%/Makefile
	@mkdir -p $(@D)
	ln -f $< $@

$(BUILD_DIR)/$(MODULES_DIR)/%.mk: $(MODULES_DIR)/gen_mod_mk.sh
	@mkdir -p $(@D)
	$(MODULES_DIR)/gen_mod_mk.sh "$(MODULES_DIR)/$*" $(BUILD_DIR)

# Kernel binary

C_FILES = $(shell find $(SRC_DIR) -name '*.c')
ASM_FILES = $(shell find $(SRC_DIR) -name '*.S')
OBJ_FILES = $(C_FILES:%=$(BUILD_DIR)/%.o) $(ASM_FILES:%=$(BUILD_DIR)/%.o)

$(BUILD_DIR)/kernel8.elf: $(SRC_DIR)/linker.ld $(OBJ_FILES) $(BUILD_DIR)/payload.o $(BOOT_MODULES:%=$(BUILD_DIR)/$(MODULES_DIR)/%.ko)
	$(CC) $(CFLAGS) -o $@ -T $^

$(IMAGE): $(BUILD_DIR)/kernel8.elf
#	$(LD) $(LDFLAGS) -T $(SRC_DIR)/linker.ld -o $(BUILD_DIR)/kernel8.elf $(OBJ_FILES) -l:crti.o -l:crtbegin.o -l:crt0.o -lbfd -lz -liberty -lc -lgcc -lsframe -l:crtend.o -l:crtn.o $(BUILD_DIR)/modules/payload.o
	$(ARMGNU)-objcopy $< -O binary $@

# Object files

$(BUILD_DIR)/%.c.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INC_FLAGS) -MMD -c $< -o $@

$(BUILD_DIR)/%.S.o: %.S
	@mkdir -p $(@D)
	$(AS) $(ASFLAGS) $(INC_FLAGS) -MMD -c $< -o $@

# Other Makefiles

ifneq (clean,$(MAKECMDGOALS))
-include $(STD_MODULES:%=$(BUILD_DIR)/$(MODULES_DIR)/%.mk)
include $(MULTI_MODULES:%=$(MODULES_DIR)/%/Makefile)
endif