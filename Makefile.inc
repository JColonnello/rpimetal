TOOLCHAIN ?= /opt/aarch64-none-elf/bin
ARMGNU ?= $(TOOLCHAIN)/aarch64-none-elf
CC=$(ARMGNU)-gcc
LD=$(ARMGNU)-gcc
AS=$(ARMGNU)-gcc

INC_DIRS = include
override INC_FLAGS += $(addprefix -I,$(INC_DIRS))
override CFLAGS += -Wall -Wno-unknown-pragmas -ffreestanding -std=gnu11 -g
override ASFLAGS += -Wall -ffreestanding
