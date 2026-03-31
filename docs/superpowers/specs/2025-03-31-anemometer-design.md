# Anemometer Wind Sensor Linux Driver Design

**Date:** 2025-03-31  
**Topic:** Linux Device Driver for Pulse/Frequency Anemometer  
**Status:** Approved for Implementation

## 1. Overview

This document describes the design for a generic Linux device driver for anemometer wind sensors that output pulse/frequency signals. The driver supports multiple sensors, runtime configuration, and works across Linux 5.4+ and 6.x kernels.

### Key Features
- Pulse/frequency input via GPIO with IRQ counting
- Sliding window averaging for smooth wind speed readings
- Configurable calibration (slope and offset)
- Multiple instantiation methods: Device Tree, ConfigFS, and character device
- Multi-sensor support with independent configurations
- Sysfs interface for configuration and data reading

## 2. Architecture

### 2.1 Component Overview

```
┌─────────────────────────────────────────────────────────────┐
│                    anemometer.c (Platform Driver)            │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐       │
│  │   Device     │  │   ConfigFS   │  │   Char Dev   │       │
│  │    Tree      │  │   Interface  │  │  (/dev/...)  │       │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘       │
│         │                 │                 │               │
│         └─────────────────┼─────────────────┘               │
│                           │                                 │
│              ┌────────────▼────────────┐                    │
│              │   Sensor Management     │                    │
│              │   (List, create, delete)│                    │
│              └────────────┬────────────┘                    │
│                           │                                 │
│              ┌────────────▼────────────┐                    │
│              │    Per-Sensor Data      │                    │
│              │  ┌───────────────────┐  │                    │
│              │  │  IRQ Handler      │  │                    │
│              │  │  (pulse counting) │  │                    │
│              │  └───────────────────┘  │                    │
│              │  ┌───────────────────┐  │                    │
│              │  │  Worker Thread    │  │                    │
│              │  │  (buffer updates) │  │                    │
│              │  └───────────────────┘  │                    │
│              │  ┌───────────────────┐  │                    │
│              │  │  Circular Buffer  │  │                    │
│              │  │  (sliding window) │  │                    │
│              │  └───────────────────┘  │                    │
│              └────────────┬────────────┘                    │
│                           │                                 │
│              ┌────────────▼────────────┐                    │
│              │   sysfs Class           │                    │
│              │  (config & data export) │                    │
│              └─────────────────────────┘                    │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

### 2.2 Sensor Data Structure

Each sensor maintains:

```c
struct anemometer_sensor {
    char name[32];                    /* Sensor identifier */
    struct gpio_desc *gpio;           /* GPIO descriptor */
    int irq;                          /* IRQ number */
    
    /* Configuration */
    u32 window_size;                  /* Averaging window in seconds */
    u32 update_interval_ms;           /* Update interval in ms */
    s32 slope_num;                    /* Slope numerator (fixed point) */
    u32 slope_den;                    /* Slope denominator */
    s32 offset;                       /* Offset in m/s (fixed point * 1000) */
    
    /* Runtime data */
    atomic_t pulse_count;             /* IRQ-safe pulse counter */
    u32 *pulse_buffer;                /* Circular buffer of pulse counts */
    u32 buffer_head;                  /* Buffer write position */
    u32 buffer_count;                 /* Number of valid samples */
    
    /* Calculated values */
    u32 frequency_millihz;            /* Frequency * 1000 */
    s32 wind_speed_um_s;              /* Wind speed in um/s */
    
    /* Threading */
    struct task_struct *worker;
    bool running;
    
    /* sysfs */
    struct device *dev;
    
    /* List management */
    struct list_head list;
};
```

### 2.3 Threading Model

**IRQ Handler (atomic context):**
- Triggered on GPIO rising edge
- Atomically increments `pulse_count`
- Minimal execution time

**Worker Thread (process context):**
- Runs every `update_interval_ms`
- Moves pulse count to circular buffer
- Calculates frequency and wind speed
- Sleeps until next interval

**Synchronization:**
- `pulse_count`: atomic operations (no lock needed)
- Buffer updates: per-sensor mutex
- Sensor list: global mutex
- RCU for safe iteration

## 3. Data Flow

### 3.1 Pulse Counting Flow

```
Hardware Pulse ──► GPIO IRQ ──► IRQ Handler ──► pulse_count++
                                                    │
                                                    ▼
Worker Thread ◄──── buffer[head] = pulse_count (atomic exchange)
       │
       ▼
sum = Σ buffer[i]  (last window_size samples)
frequency = sum / window_size
wind_speed = frequency * slope + offset
```

### 3.2 Sliding Window Algorithm

```c
/* Called every update_interval_ms */
void anemometer_update(struct anemometer_sensor *sensor)
{
    /* Atomically get and reset pulse count */
    u32 pulses = atomic_xchg(&sensor->pulse_count, 0);
    
    /* Add to circular buffer */
    sensor->pulse_buffer[sensor->buffer_head] = pulses;
    sensor->buffer_head = (sensor->buffer_head + 1) % sensor->window_size;
    if (sensor->buffer_count < sensor->window_size)
        sensor->buffer_count++;
    
    /* Calculate frequency */
    u32 sum = 0;
    for (i = 0; i < sensor->buffer_count; i++)
        sum += sensor->pulse_buffer[i];
    
    /* Convert to frequency (pulses per second) */
    u32 frequency = (sum * 1000) / (sensor->buffer_count * sensor->update_interval_ms);
    sensor->frequency_millihz = frequency;
    
    /* Apply calibration: speed = freq * slope + offset */
    s64 speed = (s64)frequency * sensor->slope_num / sensor->slope_den;
    speed += sensor->offset * 1000;  /* offset is in um/s */
    sensor->wind_speed_um_s = (s32)speed;
}
```

## 4. Interface Methods

### 4.1 Device Tree Binding

```dts
anemometer: anemometer {
    compatible = "generic,anemometer";
    status = "okay";
    
    anemometer@0 {
        label = "outdoor";
        gpios = <&gpio 23 GPIO_ACTIVE_HIGH>;
        slope = <100>;           /* 0.1 m/s per Hz * 1000 */
        slope-div = <1000>;
        offset = <0>;            /* m/s * 1000 */
        window-size = <5>;       /* seconds */
        update-interval = <1000>; /* milliseconds */
    };
    
    anemometer@1 {
        label = "indoor";
        gpios = <&gpio 24 GPIO_ACTIVE_HIGH>;
        slope = <100>;
        slope-div = <1000>;
        window-size = <10>;
    };
};
```

### 4.2 ConfigFS Interface (if CONFIG_CONFIGFS_FS=y)

```bash
# Create sensor
mkdir /sys/kernel/config/anemometer/outdoor
echo 23 > /sys/kernel/config/anemometer/outdoor/gpio
echo 100 > /sys/kernel/config/anemometer/outdoor/slope_num
echo 1000 > /sys/kernel/config/anemometer/outdoor/slope_den
echo 5 > /sys/kernel/config/anemometer/outdoor/window_size
echo 1 > /sys/kernel/config/anemometer/outdoor/enabled

# Delete sensor
rmdir /sys/kernel/config/anemometer/outdoor
```

### 4.3 Character Device Interface (Fallback)

```bash
# Add sensor
echo "add name=outdoor gpio=23 slope=0.1 window=5" > /dev/anemometer

# List sensors
echo "list" > /dev/anemometer
cat /dev/anemometer
# Output: outdoor gpio=23 freq=123.4 speed=12.34

# Delete sensor
echo "del outdoor" > /dev/anemometer
```

### 4.4 Sysfs Attributes

Per-sensor directory: `/sys/class/anemometer/<name>/`

**Read-only values:**
- `raw_pulses` - Pulses in last interval
- `frequency_hz` - Average frequency (Hz with 3 decimal places)
- `wind_speed_ms` - Wind speed in m/s (3 decimal places)
- `wind_speed_kmh` - Wind speed in km/h
- `pulse_count_total` - Total pulses since start

**Read-write configuration:**
- `slope` - Hz to m/s multiplier (format: "numerator denominator")
- `offset` - Zero offset in m/s (micrometers per second)
- `window_size` - Averaging window in seconds (1-60)
- `update_interval_ms` - Update interval in ms (100-10000)

**Control:**
- `enabled` - Start/stop sampling (0 or 1)

## 5. Calibration

The wind speed is calculated as:

```
wind_speed (m/s) = frequency (Hz) × slope + offset
```

**Default calibration:**
- slope = 0.1 m/s per Hz (10 Hz = 1 m/s)
- offset = 0 m/s

**Example calibrations:**
- Cup anemometer: slope=0.1 (typical)
- Propeller anemometer: slope=0.05 to 0.2
- Custom sensor: use calibration procedure

Calibration can be set via:
1. Device tree properties
2. ConfigFS attributes
3. Sysfs attributes (runtime)

## 6. Error Handling

### 6.1 Runtime Errors

| Error Condition | Handling |
|-----------------|----------|
| Invalid GPIO | -EINVAL on sensor creation |
| GPIO in use | -EBUSY, error message |
| IRQ registration fails | Fall back to polling mode (optional) |
| Division by zero | Return 0, log warning |
| Buffer overflow | Cap pulse count, log warning |
| Impossible frequency (>200 Hz) | Mark as error, freeze value |

### 6.2 Stale Data Detection

If no pulses detected for `2 × window_size` seconds:
- Set frequency to 0
- Set `stale` flag in sysfs
- Log warning once

### 6.3 Thread Safety

- Pulse counting: atomic operations
- Buffer access: per-sensor mutex
- Sensor list: global mutex
- sysfs reads: RCU protection

## 7. Kernel Compatibility

### 7.1 Supported Kernels

- **Minimum:** Linux 5.4 LTS
- **Maximum:** Linux 6.x (current stable)

### 7.2 API Adaptations

**GPIO API:**
- Use `gpiod_get()` / `gpiod_to_irq()` (preferred)
- Fallback to legacy `gpio_to_irq()` if needed
- Handle both APIs for 5.x and 6.x compatibility

**IRQ API:**
- Standard `request_irq()` / `free_irq()`
- Threaded IRQs optional for future enhancement

**Platform Device:**
- Compatible registration across 5.x and 6.x
- Use `platform_driver_register()`

### 7.3 Compile-Time Options

```
CONFIG_ANEMOMETER=m
CONFIG_ANEMOMETER_CONFIGFS=y   # Enable ConfigFS support
CONFIG_ANEMOMETER_DEBUG=n      # Debug messages
```

## 8. Testing Strategy

### 8.1 Unit Tests

- Mock GPIO framework
- Pulse counting accuracy
- Sliding window algorithm
- Calibration math
- Buffer overflow handling

### 8.2 Integration Tests

**Platforms:**
- Raspberry Pi 4 (BCM2711)
- BeagleBone Black (AM335x)
- Generic x86 with GPIO

**Test Cases:**
1. Device tree instantiation
2. ConfigFS create/delete
3. Char device commands
4. Multiple concurrent sensors (8+ sensors)
5. Maximum pulse rate (1000+ Hz)
6. Calibration verification
7. Error injection (invalid GPIO, etc.)

### 8.3 Performance Tests

- CPU usage at 1000 Hz pulse rate
- Memory usage with 16 sensors
- Latency: IRQ to sysfs update

## 9. File Structure

```
anemometer/
├── anemometer.c          # Main driver
├── anemometer.h          # Internal headers
├── anemometer-sysfs.c    # Sysfs implementation
├── anemometer-configfs.c # ConfigFS implementation
├── anemometer-chrdev.c   # Char device implementation
├── anemometer-dt.c       # Device tree parsing
├── Kconfig               # Kernel configuration
├── Makefile              # Build rules
└── Documentation/
    └── anemometer.rst    # Kernel documentation
```

## 10. Security Considerations

- GPIO access requires root privileges
- sysfs attributes are world-readable, root-writable
- No sensitive data exposed
- Input validation on all user-provided values

## 11. Future Enhancements

**Version 2.0 (Future):**
- IIO (Industrial I/O) subsystem integration
- Threaded IRQs for better latency
- Exponential moving average option
- Wind direction sensor support (if pulse-direction interface)
- Statistics: min/max/average over longer periods

## 12. Success Criteria

- [ ] Driver compiles on Linux 5.4 and 6.x
- [ ] Accurate pulse counting up to 1000 Hz
- [ ] Multiple sensors work independently
- [ ] All three interfaces (DT, ConfigFS, chardev) functional
- [ ] Calibration adjustable at runtime
- [ ] CPU usage < 1% at 100 Hz
- [ ] Memory usage < 100KB per sensor
- [ ] No kernel warnings or errors during normal operation

## Appendix A: Sysfs Example Session

```bash
# Load driver
modprobe anemometer

# Check sensors
ls /sys/class/anemometer/
# outdoor indoor

# Read wind speed
cat /sys/class/anemometer/outdoor/wind_speed_ms
# 12.345

# Change calibration
echo "50 1000" > /sys/class/anemometer/outdoor/slope  # 0.05 m/s per Hz

# Adjust window
echo 10 > /sys/class/anemometer/outdoor/window_size

# Read frequency
cat /sys/class/anemometer/outdoor/frequency_hz
# 246.900
```

## Appendix B: Device Tree Example

```dts
/ {
    anemometer: anemometer {
        compatible = "generic,anemometer";
        status = "okay";
        
        anemometer@0 {
            label = "outdoor";
            gpios = <&gpio 23 GPIO_ACTIVE_HIGH>;
            slope = <100>;
            slope-div = <1000>;
            offset = <0>;
            window-size = <5>;
            update-interval = <1000>;
        };
    };
};
```

---

**Document Version:** 1.0  
**Last Updated:** 2025-03-31  
**Status:** Approved for Implementation
