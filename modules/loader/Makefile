MODULE_DIR := modules/loader
SOURCE_FILES := $(shell find "$(MODULE_DIR)" -name '*.c' -or -iname '*.s' | tr '\n' ' ')
OBJ_FILES := $(SOURCE_FILES:%=$(BUILD_DIR)/%.o)
DIRS := $(shell find "$(MODULE_DIR)" -type d | tr '\n' ' ')

$(BUILD_DIR)/$(MODULE_DIR).ko: $(OBJ_FILES)
	$(CC) -r $(filter-out %.mk,$^) -o $@ -lbfd -lz -liberty -lsframe

$(BUILD_DIR)/$(MODULE_DIR).mk: $(DIRS)

-include $(OBJ_FILES:%.o=%.d)
