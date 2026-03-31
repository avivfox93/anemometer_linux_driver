# Anemometer Wind Sensor Linux Driver

A generic Linux kernel driver for pulse/frequency anemometer wind sensors.

## Overview

This driver supports anemometer sensors that output a pulse signal with frequency proportional to wind speed. It provides multiple interfaces for sensor configuration and data reading:

- **Device Tree** - Static configuration at boot time
- **Sysfs** - Runtime configuration and data reading
- **ConfigFS** - Runtime sensor creation (optional)
- **Character Device** - Runtime sensor management

## Features

- ✓ Pulse counting via GPIO interrupts
- ✓ Sliding window averaging (configurable 1-60 seconds)
- ✓ Multiple sensor support
- ✓ Configurable calibration (slope and offset)
- ✓ Wind speed in m/s and km/h
- ✓ Stale data detection
- ✓ Three instantiation methods
- ✓ Linux 5.4+ and 6.x compatible
- ✓ **Web Dashboard** - Real-time visualization (see `web_server/`)
- ✓ **Pull-up/Pull-down** - Hardware GPIO configuration
- ✓ **Debounce** - Hardware signal debouncing support

## Building

### Prerequisites

- Linux kernel headers installed
- GCC compiler
- make

#### Installing Kernel Headers

**On Ubuntu/Debian:**
```bash
sudo apt update
sudo apt install linux-headers-$(uname -r)
```

**On Raspberry Pi OS:**
```bash
sudo apt update
sudo apt install raspberrypi-kernel-headers

# If that doesn't work, try:
sudo apt install linux-headers-rpi

# Verify installation:
ls /lib/modules/$(uname -r)/build
```

**Note:** If you see "No such file or directory" for the build directory after installing headers, you may need to reboot or the headers package might not match your running kernel version.

### Compile

```bash
cd /path/to/anemometer_driver
make
```

This will produce `anemometer.ko` - the kernel module.

### Install

```bash
sudo make install
```

This will:
1. Copy `anemometer.ko` to the kernel modules directory
2. Run `depmod -a` to update module dependencies

Or manually:

```bash
sudo cp anemometer.ko /lib/modules/$(uname -r)/kernel/drivers/
sudo depmod -a
```

### Uninstall

```bash
sudo make uninstall
```

### Quick Load/Unload

```bash
# Load the module
sudo make load

# Unload the module
sudo make unload

# Reload (unload + load)
sudo make reload
```

## Usage

### Loading the Module

After installation:

```bash
sudo modprobe anemometer
# or if not installed
sudo insmod anemometer.ko
```

Check that it loaded:

```bash
lsmod | grep anemometer
dmesg | tail -20
```

### Method 1: Device Tree (Static)

Add to your device tree:

```dts
/ {
    anemometer {
        compatible = "generic,anemometer";
        
        anemometer@0 {
            label = "outdoor";
            gpios = <&gpio 23 GPIO_ACTIVE_HIGH>;
            window-size = <5>;           /* seconds */
            update-interval = <1000>;    /* milliseconds */
            slope = <100>;               /* 0.1 * 1000 */
            slope-div = <1000>;
            offset = <0>;
            pull = "up";                /* Enable pull-up for open-collector sensors */
            debounce-us = <1000>;       /* 1ms debounce to filter noise */
        };
    };
};
```

### Method 2: Character Device (Runtime)

The driver creates `/dev/anemometer` for runtime sensor management:

```bash
# Create a sensor
echo "add name=outdoor gpio=23 window=5" | sudo tee /dev/anemometer

# List sensors
cat /dev/anemometer
# Output: outdoor gpio=23 freq=123.456 speed=12.346 stale=0

# Delete sensor
echo "del outdoor" | sudo tee /dev/anemometer
```

### Method 3: ConfigFS (Runtime, Optional)

If ConfigFS is enabled in your kernel:

```bash
# Create sensor
sudo mkdir /sys/kernel/config/anemometer/outdoor

# Configure
echo 23 | sudo tee /sys/kernel/config/anemometer/outdoor/gpio
echo 100 | sudo tee /sys/kernel/config/anemometer/outdoor/slope_num
echo 1000 | sudo tee /sys/kernel/config/anemometer/outdoor/slope_den
echo 5 | sudo tee /sys/kernel/config/anemometer/outdoor/window_size
echo 1 | sudo tee /sys/kernel/config/anemometer/outdoor/enabled

# Delete sensor
sudo rmdir /sys/kernel/config/anemometer/outdoor
```

## Reading Data

Once a sensor is created, it appears in sysfs:

```bash
# List sensors
ls /sys/class/anemometer/

# Read wind speed
cat /sys/class/anemometer/outdoor/wind_speed_ms
# 12.345

cat /sys/class/anemometer/outdoor/wind_speed_kmh
# 44.442

# Read frequency
cat /sys/class/anemometer/outdoor/frequency_hz
# 123.456

# Read raw pulses
cat /sys/class/anemometer/outdoor/raw_pulses
# 123

# Check if data is stale
cat /sys/class/anemometer/outdoor/stale
# 0
```

## Calibration

Wind speed is calculated as:

```
speed (m/s) = frequency (Hz) × (slope_num / slope_den) + offset
```

Default calibration (slope = 0.1):
- 1 Hz = 0.1 m/s
- 10 Hz = 1.0 m/s
- 100 Hz = 10.0 m/s

### Adjust Calibration via Sysfs

```bash
# Read current calibration
cat /sys/class/anemometer/outdoor/slope
# 100 1000

# Set slope to 0.05 (50/1000)
echo "50 1000" | sudo tee /sys/class/anemometer/outdoor/slope

# Set offset to 0.5 m/s (500000 um/s)
echo 500000 | sudo tee /sys/class/anemometer/outdoor/offset
```

### Adjust Window Size

```bash
# Read current window
cat /sys/class/anemometer/outdoor/window_size
# 5

# Change to 10 seconds
echo 10 | sudo tee /sys/class/anemometer/outdoor/window_size
```

## Hardware Wiring

Connect your anemometer to a GPIO pin:

```
Anemometer Signal → GPIO Pin (e.g., GPIO 23)
Anemometer GND    → Ground
Anemometer VCC    → 3.3V or 5V (check sensor specs)
```

**Note:** Most pulse anemometers are open-collector/open-drain output. You can either:
- Use the driver's internal pull-up: `echo "up" | sudo tee /sys/class/anemometer/<sensor>/pull`
- Or use an external pull-up resistor (typically 10kΩ)

### Pull-up/Pull-down Configuration

Configure via device tree or sysfs:
```bash
# Enable pull-up (useful for open-collector sensors)
echo "up" | sudo tee /sys/class/anemometer/outdoor/pull

# Enable pull-down
echo "down" | sudo tee /sys/class/anemometer/outdoor/pull

# Disable pull
echo "none" | sudo tee /sys/class/anemometer/outdoor/pull
```

**Note:** Pull configuration can only be changed when the sensor is stopped. Some GPIO controllers may not support software pull configuration.

### Debounce Configuration

The driver supports hardware debouncing (if the GPIO controller supports it):

```bash
# Set debounce to 1000 microseconds (1ms)
echo 1000 | sudo tee /sys/class/anemometer/outdoor/debounce_us

# Disable debounce
echo 0 | sudo tee /sys/class/anemometer/outdoor/debounce_us
```

Debounce is useful for:
- Filtering electrical noise
- Mechanical switch bounce suppression
- Stable pulse counting in harsh environments

**Note:** Debounce configuration can only be changed when the sensor is stopped. Not all GPIO controllers support debouncing.

### Important Hardware Configuration Notes

**Pull Configuration Limitations:**
- Software pull via sysfs may not work on all GPIO controllers
- **Recommended:** Use device tree `bias-pull-up` or `bias-pull-down` properties for hardware pull configuration:
  ```dts
  gpios = <&gpio 23 GPIO_ACTIVE_HIGH | GPIO_PULL_UP>;
  ```
- Alternatively, use an external pull-up/pull-down resistor (typically 10kΩ)

**Debounce Limitations:**
- Hardware-dependent feature - not all GPIO controllers implement debouncing
- Uses kernel's `gpiod_set_debounce()` function when available
- If your GPIO controller doesn't support debouncing, consider:
  - Using a hardware RC filter on the signal line
  - Implementing debouncing in software (not in this driver)
  - Using a sensor with built-in debouncing

## Testing

Run the integration test script:

```bash
sudo ./test/test-driver.sh
```

This will:
1. Verify driver is loaded
2. Create test sensors
3. Read various attributes
4. Modify configuration
5. Clean up

## Configuration Parameters

| Parameter | Default | Range | Description |
|-----------|---------|-------|-------------|
| window-size | 5 | 1-60 | Averaging window in seconds |
| update-interval | 1000 | 100-10000 | Update interval in milliseconds |
| slope | 100/1000 | - | Hz to m/s multiplier |
| offset | 0 | - | Zero offset in μm/s |
| pull | "none" | up/down/none | GPIO pull configuration |
| debounce-us | 0 | 0+ | Debounce time in microseconds |

## Kernel Configuration

Enable the driver in kernel config:

```
Device Drivers →
    [*] Anemometer wind sensor driver
        [*]   ConfigFS interface support (optional)
```

Or via Kconfig:

```bash
CONFIG_ANEMOMETER=m
CONFIG_ANEMOMETER_CONFIGFS=y
```

## Troubleshooting

### Module won't load

Check dmesg for errors:
```bash
dmesg | grep anemometer
```

Common issues:
- GPIO not available (already in use)
- Missing kernel dependencies (GPIOLIB)

### No data in sysfs

1. Verify sensor was created:
   ```bash
   ls /sys/class/anemometer/
   ```

2. Check if IRQ is working:
   ```bash
   cat /sys/class/anemometer/<name>/raw_pulses
   ```

3. Check stale flag:
   ```bash
   cat /sys/class/anemometer/<name>/stale
   ```

### Calibration seems wrong

Verify your sensor's datasheet for the correct Hz to m/s conversion factor. Common values:
- Cup anemometer: 0.1 m/s per Hz
- Propeller anemometer: varies by model

## API Reference

### Sysfs Attributes

**Read-only:**
- `raw_pulses` - Pulses in last interval
- `frequency_hz` - Average frequency (Hz)
- `wind_speed_ms` - Wind speed in m/s
- `wind_speed_kmh` - Wind speed in km/h
- `pulse_count_total` - Total pulses since start
- `stale` - Data freshness flag

**Read-write:**
- `slope` - Calibration slope (format: "num den")
- `offset` - Calibration offset (μm/s)
- `window_size` - Averaging window (1-60 seconds)
- `update_interval_ms` - Update interval (100-10000 ms)
- `pull` - GPIO pull configuration: "up", "down", or "none" (read-only when running)
- `debounce_us` - Debounce time in microseconds, 0 to disable (read-only when running)

## Web Dashboard

A Python web server is included for real-time visualization:

```bash
cd web_server
python3 anemometer_server.py
```

Then open http://localhost:8080 in your browser.

Features:
- 📊 Real-time wind speed charts
- 📈 Historical data visualization
- 🔄 Auto-refresh with live updates
- 📱 Mobile-friendly responsive design

See `web_server/README.md` for more details.

## License

GPL-2.0

## Contributing

Contributions are welcome! Please ensure:
1. Code follows kernel coding style
2. Changes compile without warnings
3. Documentation is updated

## Support

For issues and questions, please use the GitHub issue tracker.

## Related Documentation

- Device Tree binding: `Documentation/devicetree/bindings/anemometer/anemometer.yaml`
- Design specification: `docs/superpowers/specs/2025-03-31-anemometer-design.md`
- Implementation plan: `docs/superpowers/plans/2025-03-31-anemometer-implementation.md`
