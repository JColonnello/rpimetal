STD_MODULES = testing/test testing/test2 drivers/uart drivers/mbox loader arm/irq arm/mmu-basic drivers/display drivers/timer \
sys/mux drivers/sd drivers/sd2 dummy
MULTI_MODULES = testing/test3 libc

$(BUILD_DIR)/%.ko:
	$(CC) -r $(OBJ_FILES) -o $@ $(LDLIBS)
