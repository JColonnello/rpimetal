ARMGNU ?= aarch64-none-elf
CC=$(ARMGNU)-gcc
LD=$(ARMGNU)-ld
AS=$(ARMGNU)-gcc

INC_DIRS = include
override INC_FLAGS += $(addprefix -I,$(INC_DIRS))
override CFLAGS += -Wall -Wno-unknown-pragmas -ffreestanding -nostdlib -std=gnu11 -mgeneral-regs-only -O3 
override ASFLAGS += -Wall -ffreestanding -nostdlib
