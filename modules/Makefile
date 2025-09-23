STD_MODULES = kernel testing/test testing/test2 drivers/uart drivers/mbox loader arm/irq arm/mmu-basic drivers/display drivers/usb drivers/timer \
sys/mux drivers/sd drivers/sd2 dummy fs/fat
MULTI_MODULES = testing/test3 libc

$(BUILD_DIR)/payload.o: $(MODULES:%=$(BUILD_DIR)/$(MODULES_DIR)/%.ko)
	$(LD) -r -b binary -o $@ $^