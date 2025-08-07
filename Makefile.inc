TOOLCHAIN ?= /opt/aarch64-none-elf/bin
ARMGNU ?= $(TOOLCHAIN)/aarch64-none-elf
CC=$(ARMGNU)-gcc
LD=$(ARMGNU)-ld
AS=$(ARMGNU)-gcc
AR=$(ARMGNU)-ar

RSYNC_SERVER = 192.168.0.199
INC_DIRS = include
override INC_FLAGS += $(addprefix -I,$(INC_DIRS))
override LDFLAGS += -L/opt/aarch64-none-elf/lib/gcc/aarch64-none-elf/14.2.0/ -L/opt/aarch64-none-elf/aarch64-none-elf/lib/
override CFLAGS += -Wall -Wno-unknown-pragmas -std=gnu11 -ggdb -g3 -mtp=el1 -nolibc -ftls-model=local-exec -Wno-trigraphs -march=armv8-a -mtune=cortex-a53
override ASFLAGS += -Wall -g -march=armv8-a -mtune=cortex-a53