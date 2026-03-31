# Anemometer driver Makefile

obj-m := anemometer.o
anemometer-objs := anemometer-main.o anemometer-chrdev.o anemometer-sysfs.o anemometer-dt.o anemometer-configfs.o

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

# Module installation directory
INSTALL_DIR := /lib/modules/$(shell uname -r)/kernel/drivers

# Default target: build the module
default:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

# Clean build artifacts
clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

# Install the module
install: default
	@echo "Installing anemometer.ko to $(INSTALL_DIR)..."
	@install -d $(INSTALL_DIR)
	@install -m 644 anemometer.ko $(INSTALL_DIR)/
	@echo "Running depmod..."
	@depmod -a
	@echo "Installation complete!"
	@echo ""
	@echo "Load the module with: sudo modprobe anemometer"
	@echo "Or: sudo insmod $(INSTALL_DIR)/anemometer.ko"

# Uninstall the module
uninstall:
	@echo "Removing anemometer.ko from $(INSTALL_DIR)..."
	@rm -f $(INSTALL_DIR)/anemometer.ko
	@echo "Running depmod..."
	@depmod -a
	@echo "Uninstallation complete!"

# Load the module (requires sudo)
load: default
	@echo "Loading anemometer module..."
	@sudo insmod anemometer.ko || sudo modprobe anemometer
	@echo "Module loaded. Check dmesg for output."

# Unload the module (requires sudo)
unload:
	@echo "Unloading anemometer module..."
	@sudo rmmod anemometer || true
	@echo "Module unloaded."

# Reload the module (unload + load)
reload: unload load

.PHONY: default clean install uninstall load unload reload
