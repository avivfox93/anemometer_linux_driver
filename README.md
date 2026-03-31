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

## Building

### Prerequisites

- Linux kernel headers installed
- GCC compiler
- make

### Compile

```bash
cd /path/to/anemometer_driver
make
```

This will produce `anemometer.ko` - the kernel module.

### Install

```bash
sudo make install
sudo depmod -a
```

Or manually:

```bash
sudo cp anemometer.ko /lib/modules/$(uname -r)/kernel/drivers/
sudo depmod -a
```

## Usage

### Loading the Module

```bash
sudo modprobe anemometer
# or
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

**Note:** Most pulse anemometers are open-collector/open-drain output. You may need an external pull-up resistor (typically 10kΩ) if the sensor doesn't have one built-in.

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
