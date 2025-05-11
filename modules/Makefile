STD_MODULES = kernel testing/test testing/test2 drivers/uart drivers/mbox loader arm/irq
MULTI_MODULES = testing/test3

$(BUILD_DIR)/payload.o: $(MODULES:%=$(BUILD_DIR)/$(MODULES_DIR)/%.ko)
	$(LD) -r -b binary -o $@ $^