# Bootloader, kernel, and modules to use

BOOTLOADER = bootloaders/linked
KERNEL = examples/no-libc-uart
# EXTRA_MODULES = testing/test

# Remote server configuration

RSYNC_SERVER = 192.168.0.199

# Toolchain flags

override INC_FLAGS += $(addprefix -I,$(INC_DIRS))
override LDFLAGS += -L/opt/$(TRIPLET)/lib/gcc/$(TRIPLET)/14.2.0/ -L/opt/$(TRIPLET)/$(TRIPLET)/lib/
override CFLAGS += -Wall -Wno-unknown-pragmas -std=gnu11 -ggdb -g3 -mtp=el1 -nolibc -ftls-model=local-exec -Wno-trigraphs -march=armv8-a -mtune=cortex-a53
override ASFLAGS += -Wall -g -march=armv8-a -mtune=cortex-a53
