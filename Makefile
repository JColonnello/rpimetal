DIR = $(dir $(lastword $(MAKEFILE_LIST)))

ifeq (,$(wildcard ./config.mk))
include config.example.mk
else
include config.mk
endif

# Toolchain configuration

TOOLCHAIN ?= /opt/aarch64-none-elf/bin
ARMGNU ?= $(TOOLCHAIN)/aarch64-none-elf
CC = $(ARMGNU)-gcc
LD = $(ARMGNU)-ld
AS = $(ARMGNU)-gcc
AR = $(ARMGNU)-ar

# Dirs and files

BUILD_DIR = build
MODULES_DIR = modules
INC_DIRS = include
IMAGE = output/kernel8.img

# Phony targets

.PHONY: all clean rebuild run debug uart0 toolchain undef run-vnc debug-vnc sync mux-tcp

all: $(IMAGE)

clean:
	rm -rf $(BUILD_DIR)

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
	socat TCP-LISTEN:4444,reuseaddr,fork SYSTEM:'output/multiplex config-mult.txt',nofork

toolchain: toolchain/Dockerfile
	docker build -t rpimetal-toolchain toolchain/

undef: $(BUILD_DIR)/kernel.ko
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

# Kernel image

include $(BOOTLOADER)/Makefile
include $(KERNEL)/Makefile

$(IMAGE): $(BUILD_DIR)/kernel8.elf
	$(ARMGNU)-objcopy $< -O binary $@

# Object files

$(BUILD_DIR)/%.c.o $(BUILD_DIR)/%.c.d &: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INC_FLAGS) -MMD -c $< -o $(BUILD_DIR)/$<.o

$(BUILD_DIR)/%.S.o $(BUILD_DIR)/%.S.d &: %.S
	@mkdir -p $(@D)
	$(AS) $(ASFLAGS) $(INC_FLAGS) -MMD -c $< -o $(BUILD_DIR)/$<.o

$(BUILD_DIR)/%.s.o $(BUILD_DIR)/%.s.d &: %.s
	@mkdir -p $(@D)
	$(AS) $(ASFLAGS) $(INC_FLAGS) -MMD -c $< -o $(BUILD_DIR)/$<.o

# Other Makefiles

# Tools

.PHONY:
multiplex: output/multiplex
output/multiplex: toolchain/multiplex.c
	gcc -g -o $@ $<

ifneq (clean,$(MAKECMDGOALS))
-include $(OBJ_FILES:%.o=%.d)
-include $(STD_MODULES:%=$(BUILD_DIR)/$(MODULES_DIR)/%.mk)
include $(MULTI_MODULES:%=$(MODULES_DIR)/%/Makefile)
endif