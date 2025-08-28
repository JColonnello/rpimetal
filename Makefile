include Makefile.inc

BUILD_DIR = build
MODULES_DIR = modules
IMAGE = output/kernel8.img
BOOT_SRC_DIR = src/bootloader
KERNEL = kernel
BOOT_MODULES = arm/mmu-basic libc/libc loader arm/irq drivers/timer sys/mux drivers/mbox drivers/uart
MODULES = testing/test
MODULES += $(KERNEL)

# Phony targets

.PHONY: all clean rebuild run debug uart0 toolchain undef run-vnc debug-vnc sync mux-tcp

all: $(IMAGE)

clean:
	rm -rf $(BUILD_DIR) *.img

rebuild: clean all

run: all
	qemu-system-aarch64 -M raspi3b -kernel $(IMAGE) -serial tcp:localhost:4444 -drive file=sd.img,if=sd,format=raw # -d int

debug: all
	qemu-system-aarch64 -M raspi3b -kernel $(IMAGE) -serial tcp:localhost:4444 -drive file=sd.img,if=sd,format=raw -S -s # -d int

run-vnc: all
	qemu-system-aarch64 -M raspi3b -kernel $(IMAGE) -serial tcp:localhost:4444 -drive file=sd.img,if=sd,format=raw -vnc :1,websocket=on # -d int

debug-vnc: all
	qemu-system-aarch64 -M raspi3b -kernel $(IMAGE) -serial tcp:localhost:4444 -drive file=sd.img,if=sd,format=raw -S -s -vnc :1,websocket=on # -d int

uart0:
	nc -lkvp 4444

mux-tcp:
	socat TCP-LISTEN:4444,reuseaddr,fork SYSTEM:'build/multiplex config-mult.txt',nofork

toolchain: toolchain/Dockerfile
	docker build -t rpimetal-toolchain toolchain/

undef: $(BUILD_DIR)/$(MODULES_DIR)/kernel.ko
	@$(ARMGNU)-readelf -s $< | grep UND || true

sync:
	rsync --delete -trv output/ rsync://$(RSYNC_SERVER):873/volume/

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

# Bootloader binary

C_FILES = $(shell find $(BOOT_SRC_DIR) -iname '*.c')
ASM_FILES = $(shell find $(BOOT_SRC_DIR) -iname '*.s')
OBJ_FILES = $(C_FILES:%=$(BUILD_DIR)/%.o) $(ASM_FILES:%=$(BUILD_DIR)/%.o)

$(BUILD_DIR)/kernel8.elf: $(BOOT_SRC_DIR)/linker.ld $(OBJ_FILES) $(BUILD_DIR)/payload.o $(BOOT_MODULES:%=$(BUILD_DIR)/$(MODULES_DIR)/%.ko)
	$(CC) $(CFLAGS) -Wl,--unresolved-symbols=ignore-all -o $@ -T $^

$(IMAGE): $(BUILD_DIR)/kernel8.elf
#	$(LD) $(LDFLAGS) -T $(BOOT_SRC_DIR)/linker.ld -o $(BUILD_DIR)/kernel8.elf $(OBJ_FILES) -l:crti.o -l:crtbegin.o -l:crt0.o -lbfd -lz -liberty -lc -lgcc -lsframe -l:crtend.o -l:crtn.o $(BUILD_DIR)/modules/payload.o
	$(ARMGNU)-objcopy $< -O binary $@

# Object files

$(BUILD_DIR)/%.c.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INC_FLAGS) -MMD -c $< -o $@

$(BUILD_DIR)/%.S.o: %.S
	@mkdir -p $(@D)
	$(AS) $(ASFLAGS) $(INC_FLAGS) -MMD -c $< -o $@

$(BUILD_DIR)/%.s.o: %.s
	@mkdir -p $(@D)
	$(AS) $(ASFLAGS) $(INC_FLAGS) -MMD -c $< -o $@

# Other Makefiles

# Tools

.PHONY:
multiplex: $(BUILD_DIR)/multiplex
$(BUILD_DIR)/multiplex: toolchain/multiplex.c
	gcc -g -o $@ $<

ifneq (clean,$(MAKECMDGOALS))
-include $(OBJ_FILES:%.o=%.d)
-include $(STD_MODULES:%=$(BUILD_DIR)/$(MODULES_DIR)/%.mk)
include $(MULTI_MODULES:%=$(MODULES_DIR)/%/Makefile)
endif