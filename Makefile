include Makefile.inc

BUILD_DIR = build
SRC_DIR = src
KERNEL = kernel8.img

C_FILES = $(shell find $(SRC_DIR) -name '*.c')
ASM_FILES = $(shell find $(SRC_DIR) -name '*.S')
OBJ_FILES = $(C_FILES:%=$(BUILD_DIR)/%.o)
OBJ_FILES += $(ASM_FILES:%=$(BUILD_DIR)/%.o)

all: $(KERNEL)

clean:
	rm -rf $(BUILD_DIR) *.img 

$(BUILD_DIR)/%.c.o: %.c
	mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INC_FLAGS) -MMD -c $< -o $@

$(BUILD_DIR)/%.S.o: %.S
	$(AS) $(ASFLAGS) $(INC_FLAGS) -MMD -c $< -o $@

$(KERNEL): $(SRC_DIR)/linker.ld $(OBJ_FILES) $(BUILD_DIR)/modules/payload.o
	$(LD) $(CFLAGS) -T $(SRC_DIR)/linker.ld -o $(BUILD_DIR)/kernel8.elf $(OBJ_FILES) -lbfd -lz -liberty -lsframe $(BUILD_DIR)/modules/payload.o
	$(ARMGNU)-objcopy $(BUILD_DIR)/kernel8.elf -O binary kernel8.img

run: all
	qemu-system-aarch64 -M raspi3b -kernel kernel8.img -serial tcp:localhost:4444 -nographic -d int

debug: all
	qemu-system-aarch64 -M raspi3b -kernel kernel8.img -serial tcp:localhost:4444 -nographic -d int -S -s

uart0:
	nc -lkvp 4444

toolchain: toolchain/Dockerfile
	docker buildx build toolchain/

-include $(OBJ_FILES:%.o=%.d)
-include modules/Makefile

.PHONY: all run uart0 debug toolchain