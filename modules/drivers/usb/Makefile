MODULE_DIR := modules/drivers/usb
SOURCE_FILES := $(shell find -L "$(MODULE_DIR)" -name '*.c' -or -iname '*.s' | tr '\n' ' ')
OBJ_FILES := $(SOURCE_FILES:%=$(BUILD_DIR)/%.o)
DIRS := $(shell find "$(MODULE_DIR)" -type d | tr '\n' ' ')

$(BUILD_DIR)/$(MODULE_DIR)/%: INC_FLAGS := $(INC_FLAGS) -I$(MODULE_DIR)/override -I$(MODULE_DIR)/include
$(BUILD_DIR)/$(MODULE_DIR)/%: CFLAGS += -DAARCH64=1 -DRASPPI=3

$(BUILD_DIR)/$(MODULE_DIR).ko: $(OBJ_FILES)
	$(CC) -r $(filter-out %.mk,$^) -o $@

$(BUILD_DIR)/$(MODULE_DIR).mk: $(DIRS)

-include $(OBJ_FILES:%.o=%.d)
