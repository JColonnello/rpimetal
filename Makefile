# Toolchain flags

override INC_FLAGS += $(addprefix -I,$(INC_DIRS))
override LDFLAGS += -L/opt/$(TRIPLET)/lib/gcc/$(TRIPLET)/14.2.0/ -L/opt/$(TRIPLET)/$(TRIPLET)/lib/
override CFLAGS += -Wall -Wno-unknown-pragmas -std=gnu11 -ggdb -g3 -mtp=el1 -nolibc -ftls-model=local-exec -Wno-trigraphs -march=armv8-a -mtune=cortex-a53
override ASFLAGS += -Wall -g -march=armv8-a -mtune=cortex-a53

# Toolchain location

TRIPLET = aarch64-none-elf
ARMGNU ?= /opt/$(TRIPLET)/bin/$(TRIPLET)
CC := $(ARMGNU)-gcc
LD := $(ARMGNU)-ld
AS := $(ARMGNU)-gcc
AR := $(ARMGNU)-ar

# Include user configuration

ifeq (,$(wildcard ./config.mk))
include config.example.mk
else
include config.mk
endif

# Dirs and files

BUILD_DIR = build
MODULES_DIR = modules
INC_DIRS = include
IMAGE = output/kernel8.img
SD = sd.img

# Phony targets

.PHONY: all clean rebuild run debug uart0 toolchain undef run-vnc debug-vnc sync mux-tcp

all: $(IMAGE) $(SD)

clean:
	rm -rf $(BUILD_DIR)

rebuild:
	$(MAKE) clean
	$(MAKE) all

run: all
	qemu-system-aarch64 -M raspi3b -kernel $(IMAGE) -serial tcp:localhost:4444 -drive file=$(SD),if=sd,format=raw # -d int

debug: all
	qemu-system-aarch64 -M raspi3b -kernel $(IMAGE) -serial tcp:localhost:4444 -drive file=$(SD),if=sd,format=raw -S -s # -d int

run-vnc: all
	qemu-system-aarch64 -M raspi3b -kernel $(IMAGE) -serial tcp:localhost:4444 -drive file=$(SD),if=sd,format=raw -vnc :1,websocket=on # -d int

debug-vnc: all
	qemu-system-aarch64 -M raspi3b -kernel $(IMAGE) -serial tcp:localhost:4444 -drive file=$(SD),if=sd,format=raw -S -s -vnc :1,websocket=on # -d int

uart0:
	nc -lkvp 4444

mux-tcp: output/multiplex
	socat TCP-LISTEN:4444,reuseaddr,fork SYSTEM:'output/multiplex config-mult.txt',nofork

toolchain: toolchain/Dockerfile
	docker build -t rpimetal-toolchain toolchain/

undef: $(BUILD_DIR)/kernel.ko
	@$(ARMGNU)-readelf -s $< | grep UND || true

sync:
	rsync --delete -trv output/ rsync://$(RSYNC_SERVER):873/volume/

# General variables

DIR = $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))
$(BUILD_DIR)/%: DIR = $(basename $(@:$(BUILD_DIR)/%=%))
OUTPUT_DIR = $(BUILD_DIR)/$(DIR)
OUTPUT_KO = $(OUTPUT_DIR).ko
SOURCE_FILES = $(shell find "$(DIR)" -name '*.c' -or -iname '*.s')
OBJ_FILES = $(SOURCE_FILES:%=$(BUILD_DIR)/%.o)

# Empty recipes

# Module file and recipe

include $(MODULES_DIR)/Makefile

# Kernel image

-include $(KERNEL)/Makefile
include $(BOOTLOADER)/Makefile

$(IMAGE): $(BUILD_DIR)/kernel8.elf
	@mkdir -p $(@D)
	$(ARMGNU)-objcopy $< -O binary $@

# Disk image

$(SD):
	truncate -s 64M $@
	mkfs.vfat -F 32 $@

# Object files

$(BUILD_DIR)/%.c.o : %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INC_FLAGS) -MMD -c $< -o $(BUILD_DIR)/$<.o

$(BUILD_DIR)/%.S.o : %.S
	@mkdir -p $(@D)
	$(AS) $(ASFLAGS) $(INC_FLAGS) -MMD -c $< -o $(BUILD_DIR)/$<.o

$(BUILD_DIR)/%.s.o : %.s
	@mkdir -p $(@D)
	$(AS) $(ASFLAGS) $(INC_FLAGS) -MMD -c $< -o $(BUILD_DIR)/$<.o

# Tools

.PHONY:
multiplex: output/multiplex
output/multiplex: toolchain/multiplex.c
	gcc -g -o $@ $<

# Other Makefiles

$(MULTI_MODULES:%=$(BUILD_DIR)/$(MODULES_DIR)/%.ko):
	@echo "$@ cannot be built because the default .ko recipe does not apply to multi-modules"
	@exit 1

ifneq (clean,$(MAKECMDGOALS))
include $(shell [ -d $(BUILD_DIR) ] && find $(BUILD_DIR) -name '*.d')
-include $(STD_MODULES:%=$(MODULES_DIR)/%/Makefile)
include $(MULTI_MODULES:%=$(MODULES_DIR)/%/Makefile)
endif

.SECONDEXPANSION:
$(BUILD_DIR)/%.ko: $$(OBJ_FILES)
	@mkdir -p $(@D)
	$(CC) -r $(OBJ_FILES) $(LDLIBS) -o $@ 
