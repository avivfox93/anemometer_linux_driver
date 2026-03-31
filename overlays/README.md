# Device Tree Overlays

This directory contains Device Tree overlays for various hardware configurations.

## Available Overlays

### `anemometer-gpio26.dts`

Raspberry Pi overlay for anemometer on GPIO 26 with pull-up enabled.

**Hardware:**
- GPIO: GPIO 26 (Physical Pin 37)
- Pull-up: Enabled (internal)
- Debounce: Disabled (hardware-dependent)

**Installation:**

1. **Copy and compile:**
   ```bash
   sudo cp overlays/anemometer-gpio26.dts /boot/overlays/
   sudo dtc -I dts -O dtb -o /boot/overlays/anemometer-gpio26.dtbo /boot/overlays/anemometer-gpio26.dts
   ```

2. **Enable in `/boot/config.txt`:**
   ```
   # Add at the end of the file:
   dtoverlay=anemometer-gpio26
   ```

3. **Reboot:**
   ```bash
   sudo reboot
   ```

4. **Verify:**
   ```bash
   ls /sys/class/anemometer/
   # Should show: wind
   
   cat /sys/class/anemometer/wind/wind_speed_ms
   ```

## Creating Custom Overlays

To create an overlay for a different GPIO:

1. Copy the example: `cp anemometer-gpio26.dts my-overlay.dts`
2. Change the GPIO number:
   - `brcm,pins = <26>;` → `brcm,pins = <XX>;`
   - `gpios = <&gpio 26 ...>` → `gpios = <&gpio XX ...>;`
3. Adjust pull configuration:
   - `brcm,pull = <2>;` (2 = up, 1 = down, 0 = none)
4. Compile and install

## Hardware Connection

For GPIO 26 (Physical Pin 37):

```
Raspberry Pi          Anemometer
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
Pin 37 (GPIO 26)  →   Signal
Pin 39 (GND)      →   GND
Pin 1 or 2 (3V3/5V) → VCC (check sensor specs)
```

The overlay configures GPIO 26 with internal pull-up, so no external resistor is needed for open-collector sensors.

## Troubleshooting

### "Failed to apply overlay" error
- Check `sudo vcdbg log msg` for errors
- Ensure overlay syntax is correct
- Verify GPIO is not already in use: `raspi-gpio get 26`

### GPIO conflict
Some GPIOs are reserved by system functions:
- GPIO 0-1: I2C (ID_SD/ID_SC)
- GPIO 14-15: UART (TX/RX)
- GPIO 18: PCM_CLK

Use `raspi-gpio get` to check GPIO availability.

## GPIO Reference (Raspberry Pi 4)

| GPIO | Physical Pin | Notes |
|------|--------------|-------|
| 4    | 7            | General purpose |
| 17   | 11           | General purpose |
| 27   | 13           | General purpose |
| 22   | 15           | General purpose |
| 5    | 29           | General purpose |
| 6    | 31           | General purpose |
| 13   | 33           | PWM1 |
| 19   | 35           | PCM_FS |
| 26   | 37           | **Recommended for anemometer** |
| 21   | 40           | PCM_DIN |
| 20   | 38           | PCM_DOUT |
| 16   | 36           | General purpose |
| 12   | 32           | PWM0 |
| 7    | 26           | SPI0_CE1 |
| 8    | 24           | SPI0_CE0 |
| 25   | 22           | General purpose |
| 24   | 18           | SPI0_CE0 |
| 23   | 16           | SPI0_MOSI |
