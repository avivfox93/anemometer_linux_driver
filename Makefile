# SPDX-License-Identifier: GPL-2.0
#
# Makefile for the anemometer wind sensor driver

obj-$(CONFIG_ANEMOMETER) += anemometer.o

anemometer-y := anemometer-main.o \
		anemometer-chrdev.o \
		anemometer-sysfs.o \
		anemometer-dt.o

# Conditionally include ConfigFS support
anemometer-$(CONFIG_ANEMOMETER_CONFIGFS) += anemometer-configfs.o
