savedcmd_anemometer.mod := printf '%s\n'   anemometer-main.o anemometer-chrdev.o anemometer-sysfs.o anemometer-dt.o anemometer-configfs.o | awk '!x[$$0]++ { print("./"$$0) }' > anemometer.mod
