VARIANTS := test3a test3b test3c

$(VARIANTS:%=$(BUILD_DIR)/$(MODULES_DIR)/%.ko): $(BUILD_DIR)/$(MODULES_DIR)/test3%.ko: modules/test3/Makefile
	echo $* > $@