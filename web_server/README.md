# Anemometer Web Server

A Python HTTP server that provides real-time visualization of wind data from the anemometer driver.

## Features

- 🌐 Web-based dashboard with real-time charts
- 📊 Multiple data views (m/s, km/h, **knots**, frequency)
- 📈 Historical data graph (configurable time range)
- 🔄 Auto-refresh with Server-Sent Events (SSE)
- 📱 Responsive design (works on mobile)
- 🔌 Multiple sensor support
- ⚓ Nautical units (knots) for marine applications

## Requirements

- Python 3.7+
- anemometer kernel driver loaded
- At least one sensor configured

## Usage

### Start the Server

```bash
cd web_server
python3 anemometer_server.py
```

### Access the Dashboard

Open your browser and go to:
```
http://localhost:8080
```

### Command Line Options

```bash
# Specify port
python3 anemometer_server.py --port 3000

# Monitor specific sensor
python3 anemometer_server.py --sensor outdoor

# Change update interval
python3 anemometer_server.py --interval 2.0
```

## API Endpoints

The server provides a REST API for accessing data:

### GET /api/sensors
List available sensors
```json
{
  "sensors": ["outdoor", "indoor"]
}
```

### GET /api/current
Get current reading from the monitored sensor
```json
{
  "wind_speed_ms": 12.345,
  "wind_speed_kmh": 44.442,
  "frequency_hz": 123.456,
  "raw_pulses": 123,
  "stale": false,
  "timestamp": "2025-03-31T21:45:00"
}
```

### GET /api/data?limit=100
Get historical data (last N samples)
```json
{
  "sensor": "outdoor",
  "timestamps": [...],
  "speed_ms": [...],
  "speed_kmh": [...],
  "speed_knots": [...],
  "frequency": [...]
}
```

### GET /api/events
Server-Sent Events stream for real-time updates
```
data: {"timestamp": "...", "speed_ms": 12.345, ...}
```

## Dashboard Features

### Real-time Cards
- Wind Speed (m/s)
- Wind Speed (km/h)
- Wind Speed (knots) - *perfect for nautical use*
- Pulse Frequency (Hz)

### Live Chart
- Dual Y-axis (wind speed + frequency)
- Last 60 seconds of data
- Smooth animations
- Auto-updating

### Controls
- Sensor selector dropdown
- Manual refresh button
- Live status indicator
- Last update timestamp

## Screenshots

The dashboard features:
- Modern gradient background
- Card-based layout
- Responsive charts using Chart.js
- Live status indicators
- Mobile-friendly design

## Troubleshooting

### "No sensors found" error
Make sure the anemometer driver is loaded:
```bash
sudo modprobe anemometer
# or
sudo insmod anemometer.ko
```

Check available sensors:
```bash
ls /sys/class/anemometer/
```

### Cannot access port
If using port 80 or other low ports, run with sudo:
```bash
sudo python3 anemometer_server.py --port 80
```

Or use a higher port (8080, 3000, etc.) without sudo.

### No data updating
Check if the sensor is providing data:
```bash
cat /sys/class/anemometer/<sensor>/wind_speed_ms
```

If it shows "0.000", check:
1. Hardware wiring
2. GPIO configuration
3. Sensor calibration

## Architecture

```
┌─────────────────┐     ┌──────────────┐     ┌─────────────┐
│   Browser       │────>│  HTTP Server │────>│  Sysfs API  │
│   (Dashboard)   │<────│  (Python)    │<────│  (/sys/...) │
└─────────────────┘     └──────────────┘     └─────────────┘
         │                       │
         │              ┌────────▼────────┐
         │              │  Data Collector │
         └──────────────│  (Background)   │
                        └─────────────────┘
```

## Development

The server uses:
- Python's built-in `http.server` (no external dependencies!)
- Chart.js (loaded via CDN)
- Server-Sent Events for real-time updates
- Threading for background data collection

To modify the dashboard, edit the HTML/CSS in the `anemometer_server.py` file (look for `INDEX_HTML`).

## License

GPL-2.0 (same as the anemometer driver)
