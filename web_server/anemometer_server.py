#!/usr/bin/env python3
"""
Anemometer Wind Data Web Server

A lightweight HTTP server that reads wind data from the anemometer driver
and displays it with real-time charts.

Usage:
    python3 anemometer_server.py [--port PORT] [--sensor SENSOR]

Requirements:
    - Python 3.7+
    - anemometer driver loaded with at least one sensor
"""

import os
import sys
import json
import time
import argparse
import threading
from pathlib import Path
from datetime import datetime
from http.server import HTTPServer, BaseHTTPRequestHandler
from urllib.parse import urlparse, parse_qs

# Configuration
SYSFS_BASE = Path("/sys/class/anemometer")
DEFAULT_PORT = 8080
DEFAULT_SENSOR = None  # Auto-detect first sensor
MAX_HISTORY = 3600  # Keep 1 hour of data (1 sample/sec)

# Global data storage
wind_data = {
    "timestamps": [],
    "speed_ms": [],
    "speed_kmh": [],
    "speed_knots": [],
    "frequency": [],
    "sensor_name": None,
    "last_update": None
}

data_lock = threading.Lock()


def get_sensors():
    """Get list of available sensors."""
    if not SYSFS_BASE.exists():
        return []
    return [d.name for d in SYSFS_BASE.iterdir() if d.is_dir()]


def read_sensor_data(sensor_name):
    """Read current data from sensor sysfs."""
    sensor_path = SYSFS_BASE / sensor_name
    
    try:
        data = {}
        
        # Read all attributes
        for attr in ["wind_speed_ms", "wind_speed_kmh", "frequency_hz", 
                     "raw_pulses", "stale", "pulse_count_total"]:
            try:
                with open(sensor_path / attr, "r") as f:
                    value = f.read().strip()
                    # Parse numeric values
                    if attr in ["wind_speed_ms", "wind_speed_kmh", "frequency_hz"]:
                        data[attr] = float(value)
                    elif attr in ["raw_pulses", "pulse_count_total"]:
                        data[attr] = int(value)
                    elif attr == "stale":
                        data[attr] = bool(int(value))
            except (IOError, ValueError) as e:
                data[attr] = None
        
        data["timestamp"] = datetime.now().isoformat()
        return data
        
    except Exception as e:
        print(f"Error reading sensor {sensor_name}: {e}", file=sys.stderr)
        return None


def data_collector(sensor_name, interval=1.0):
    """Background thread to collect data from sensor."""
    global wind_data
    
    print(f"Starting data collector for sensor: {sensor_name}")
    
    while True:
        try:
            data = read_sensor_data(sensor_name)
            if data:
                with data_lock:
                    wind_data["sensor_name"] = sensor_name
                    wind_data["timestamps"].append(data["timestamp"])
                    speed_ms = data.get("wind_speed_ms", 0)
                    wind_data["speed_ms"].append(speed_ms)
                    wind_data["speed_kmh"].append(data.get("wind_speed_kmh", 0))
                    # Convert m/s to knots: 1 knot = 0.514444 m/s
                    wind_data["speed_knots"].append(speed_ms * 1.94384)
                    wind_data["frequency"].append(data.get("frequency_hz", 0))
                    wind_data["last_update"] = data["timestamp"]
                    
                    # Keep only last MAX_HISTORY points
                    if len(wind_data["timestamps"]) > MAX_HISTORY:
                        wind_data["timestamps"] = wind_data["timestamps"][-MAX_HISTORY:]
                        wind_data["speed_ms"] = wind_data["speed_ms"][-MAX_HISTORY:]
                        wind_data["speed_kmh"] = wind_data["speed_kmh"][-MAX_HISTORY:]
                        wind_data["speed_knots"] = wind_data["speed_knots"][-MAX_HISTORY:]
                        wind_data["frequency"] = wind_data["frequency"][-MAX_HISTORY:]
                        
        except Exception as e:
            print(f"Collector error: {e}", file=sys.stderr)
            
        time.sleep(interval)


class AnemometerHandler(BaseHTTPRequestHandler):
    """HTTP request handler for anemometer web server."""
    
    def log_message(self, format, *args):
        """Override to reduce logging noise."""
        if "/api/" not in args[0]:  # Only log non-API requests
            super().log_message(format, *args)
    
    def do_GET(self):
        """Handle GET requests."""
        parsed_path = urlparse(self.path)
        path = parsed_path.path
        
        # Serve API endpoints
        if path == "/api/sensors":
            self.serve_sensors()
        elif path == "/api/data":
            self.serve_data()
        elif path == "/api/current":
            self.serve_current()
        elif path == "/api/events":
            self.serve_events()
        # Serve static files
        elif path == "/" or path == "/index.html":
            self.serve_index()
        elif path == "/chart.js":
            self.serve_chart_js()
        elif path == "/style.css":
            self.serve_css()
        else:
            self.send_error(404)
    
    def serve_index(self):
        """Serve main HTML page."""
        self.send_response(200)
        self.send_header("Content-Type", "text/html")
        self.end_headers()
        self.wfile.write(INDEX_HTML.encode())
    
    def serve_chart_js(self):
        """Serve Chart.js library (CDN fallback in HTML)."""
        self.send_response(200)
        self.send_header("Content-Type", "application/javascript")
        self.end_headers()
        self.wfile.write(b"// Chart.js served via CDN in HTML")
    
    def serve_css(self):
        """Serve CSS styles."""
        self.send_response(200)
        self.send_header("Content-Type", "text/css")
        self.end_headers()
        self.wfile.write(STYLE_CSS.encode())
    
    def serve_sensors(self):
        """API: List available sensors."""
        sensors = get_sensors()
        self.send_json({"sensors": sensors})
    
    def serve_data(self):
        """API: Get historical data."""
        query = parse_qs(urlparse(self.path).query)
        limit = int(query.get("limit", [100])[0])
        
        with data_lock:
            response = {
                "sensor": wind_data["sensor_name"],
                "timestamps": wind_data["timestamps"][-limit:],
                "speed_ms": wind_data["speed_ms"][-limit:],
                "speed_kmh": wind_data["speed_kmh"][-limit:],
                "speed_knots": wind_data["speed_knots"][-limit:],
                "frequency": wind_data["frequency"][-limit:]
            }
        
        self.send_json(response)
    
    def serve_current(self):
        """API: Get current reading."""
        sensor = wind_data.get("sensor_name")
        if not sensor:
            self.send_json({"error": "No sensor configured"}, 503)
            return
            
        data = read_sensor_data(sensor)
        if data:
            self.send_json(data)
        else:
            self.send_json({"error": "Failed to read sensor"}, 503)
    
    def serve_events(self):
        """API: Server-Sent Events for real-time updates."""
        self.send_response(200)
        self.send_header("Content-Type", "text/event-stream")
        self.send_header("Cache-Control", "no-cache")
        self.send_header("Connection", "keep-alive")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.end_headers()
        
        last_sent = None
        try:
            while True:
                with data_lock:
                    current = wind_data.get("last_update")
                    if current and current != last_sent:
                        last_sent = current
                        data = {
                            "timestamp": current,
                            "speed_ms": wind_data["speed_ms"][-1] if wind_data["speed_ms"] else 0,
                            "speed_kmh": wind_data["speed_kmh"][-1] if wind_data["speed_kmh"] else 0,
                            "speed_knots": wind_data["speed_knots"][-1] if wind_data["speed_knots"] else 0,
                            "frequency": wind_data["frequency"][-1] if wind_data["frequency"] else 0
                        }
                        event = f"data: {json.dumps(data)}\n\n"
                        self.wfile.write(event.encode())
                        self.wfile.flush()
                
                time.sleep(0.5)
                
        except (BrokenPipeError, ConnectionResetError):
            pass  # Client disconnected
    
    def send_json(self, data, status=200):
        """Send JSON response."""
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.end_headers()
        self.wfile.write(json.dumps(data).encode())


# HTML Template
INDEX_HTML = '''<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Anemometer Wind Monitor</title>
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }
        
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Oxygen, Ubuntu, Cantarell, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            padding: 20px;
        }
        
        .container {
            max-width: 1200px;
            margin: 0 auto;
        }
        
        header {
            text-align: center;
            color: white;
            margin-bottom: 30px;
        }
        
        h1 {
            font-size: 2.5em;
            margin-bottom: 10px;
            text-shadow: 2px 2px 4px rgba(0,0,0,0.3);
        }
        
        .subtitle {
            opacity: 0.9;
            font-size: 1.1em;
        }
        
        .dashboard {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
            gap: 20px;
            margin-bottom: 30px;
        }
        
        .card {
            background: white;
            border-radius: 15px;
            padding: 25px;
            box-shadow: 0 10px 30px rgba(0,0,0,0.2);
            transition: transform 0.3s ease;
        }
        
        .card:hover {
            transform: translateY(-5px);
        }
        
        .card-title {
            font-size: 0.9em;
            color: #666;
            text-transform: uppercase;
            letter-spacing: 1px;
            margin-bottom: 10px;
        }
        
        .card-value {
            font-size: 3em;
            font-weight: bold;
            color: #333;
        }
        
        .card-unit {
            font-size: 1.2em;
            color: #999;
            margin-left: 5px;
        }
        
        .wind-speed { color: #667eea; }
        .wind-speed-kmh { color: #f093fb; }
        .wind-speed-knots { color: #4facfe; }
        .frequency { color: #f5576c; }
        
        .chart-container {
            background: white;
            border-radius: 15px;
            padding: 25px;
            box-shadow: 0 10px 30px rgba(0,0,0,0.2);
            margin-bottom: 20px;
        }
        
        .chart-title {
            font-size: 1.3em;
            margin-bottom: 20px;
            color: #333;
        }
        
        .controls {
            background: white;
            border-radius: 15px;
            padding: 20px;
            box-shadow: 0 10px 30px rgba(0,0,0,0.2);
            display: flex;
            gap: 15px;
            align-items: center;
            flex-wrap: wrap;
        }
        
        select, button {
            padding: 10px 20px;
            border: 2px solid #667eea;
            border-radius: 8px;
            font-size: 1em;
            cursor: pointer;
            transition: all 0.3s;
        }
        
        select {
            background: white;
            color: #333;
        }
        
        button {
            background: #667eea;
            color: white;
            border: none;
        }
        
        button:hover {
            background: #5a6fd6;
            transform: translateY(-2px);
        }
        
        .status {
            display: inline-flex;
            align-items: center;
            gap: 8px;
            padding: 8px 15px;
            border-radius: 20px;
            font-size: 0.9em;
            font-weight: 500;
        }
        
        .status.online {
            background: #d4edda;
            color: #155724;
        }
        
        .status.offline {
            background: #f8d7da;
            color: #721c24;
        }
        
        .status-dot {
            width: 8px;
            height: 8px;
            border-radius: 50%;
            background: currentColor;
            animation: pulse 2s infinite;
        }
        
        @keyframes pulse {
            0%, 100% { opacity: 1; }
            50% { opacity: 0.5; }
        }
        
        .last-update {
            margin-left: auto;
            color: #666;
            font-size: 0.9em;
        }
        
        @media (max-width: 768px) {
            h1 { font-size: 1.8em; }
            .card-value { font-size: 2em; }
        }
    </style>
</head>
<body>
    <div class="container">
        <header>
            <h1>🌬️ Anemometer Monitor</h1>
            <div class="subtitle">Real-time Wind Speed Monitoring</div>
        </header>
        
        <div class="dashboard">
            <div class="card">
                <div class="card-title">Wind Speed</div>
                <div class="card-value wind-speed">
                    <span id="speed-ms">-->/span>
                    <span class="card-unit">m/s</span>
                </div>
            </div>
            
            <div class="card">
                <div class="card-title">Wind Speed</div>
                <div class="card-value wind-speed-kmh">
                    <span id="speed-kmh">--></span>
                    <span class="card-unit">km/h</span>
                </div>
            </div>

            <div class="card">
                <div class="card-title">Wind Speed</div>
                <div class="card-value wind-speed-knots">
                    <span id="speed-knots">--></span>
                    <span class="card-unit">knots</span>
                </div>
            </div>

            <div class="card">
                <div class="card-title">Frequency</div>
                <div class="card-value frequency">
                    <span id="frequency">-->/span>
                    <span class="card-unit">Hz</span>
                </div>
            </div>
        </div>
        
        <div class="chart-container">
            <div class="chart-title">Wind Speed History</div>
            <canvas id="windChart"></canvas>
        </div>
        
        <div class="controls">
            <select id="sensor-select">
                <option>Loading sensors...</option>
            </select>
            
            <button onclick="refreshData()">🔄 Refresh</button>
            
            <div class="status online" id="status">
                <span class="status-dot"></span>
                <span id="status-text">Live</span>
            </div>
            
            <div class="last-update" id="last-update">
                Last update: Never
            </div>
        </div>
    </div>
    
    <script>
        // Chart setup
        const ctx = document.getElementById('windChart').getContext('2d');
        const windChart = new Chart(ctx, {
            type: 'line',
            data: {
                labels: [],
                datasets: [{
                    label: 'Wind Speed (m/s)',
                    data: [],
                    borderColor: '#667eea',
                    backgroundColor: 'rgba(102, 126, 234, 0.1)',
                    borderWidth: 2,
                    fill: true,
                    tension: 0.4
                }, {
                    label: 'Frequency (Hz)',
                    data: [],
                    borderColor: '#4facfe',
                    backgroundColor: 'rgba(79, 172, 254, 0.1)',
                    borderWidth: 2,
                    fill: false,
                    tension: 0.4,
                    yAxisID: 'y1'
                }]
            },
            options: {
                responsive: true,
                interaction: {
                    mode: 'index',
                    intersect: false
                },
                plugins: {
                    legend: {
                        position: 'top'
                    }
                },
                scales: {
                    x: {
                        display: true,
                        title: {
                            display: true,
                            text: 'Time'
                        }
                    },
                    y: {
                        display: true,
                        title: {
                            display: true,
                            text: 'Wind Speed (m/s)'
                        },
                        position: 'left'
                    },
                    y1: {
                        display: true,
                        title: {
                            display: true,
                            text: 'Frequency (Hz)'
                        },
                        position: 'right',
                        grid: {
                            drawOnChartArea: false
                        }
                    }
                },
                animation: {
                    duration: 300
                }
            }
        });
        
        // Load available sensors
        async function loadSensors() {
            try {
                const response = await fetch('/api/sensors');
                const data = await response.json();
                
                const select = document.getElementById('sensor-select');
                select.innerHTML = '';
                
                if (data.sensors.length === 0) {
                    select.innerHTML = '<option>No sensors found</option>';
                    updateStatus(false, 'No sensors');
                    return;
                }
                
                data.sensors.forEach(sensor => {
                    const option = document.createElement('option');
                    option.value = sensor;
                    option.textContent = sensor;
                    select.appendChild(option);
                });
                
                updateStatus(true, 'Live');
                refreshData();
                
            } catch (error) {
                console.error('Failed to load sensors:', error);
                updateStatus(false, 'Error');
            }
        }
        
        // Update status indicator
        function updateStatus(online, text) {
            const status = document.getElementById('status');
            const statusText = document.getElementById('status-text');
            
            status.className = 'status ' + (online ? 'online' : 'offline');
            statusText.textContent = text;
        }
        
        // Refresh current data
        async function refreshData() {
            try {
                const response = await fetch('/api/current');
                const data = await response.json();
                
                if (data.error) {
                    updateStatus(false, 'Error');
                    return;
                }
                
                document.getElementById('speed-ms').textContent = data.wind_speed_ms?.toFixed(3) || '--';
                document.getElementById('speed-kmh').textContent = data.wind_speed_kmh?.toFixed(3) || '--';
                document.getElementById('speed-knots').textContent = data.wind_speed_knots?.toFixed(3) || '--';
                document.getElementById('frequency').textContent = data.frequency_hz?.toFixed(3) || '--';
                document.getElementById('last-update').textContent = 
                    'Last update: ' + new Date().toLocaleTimeString();
                
                updateStatus(true, 'Live');
                
            } catch (error) {
                console.error('Failed to refresh:', error);
                updateStatus(false, 'Offline');
            }
        }
        
        // Load historical data
        async function loadHistory() {
            try {
                const response = await fetch('/api/data?limit=60');
                const data = await response.json();
                
                if (data.timestamps) {
                    windChart.data.labels = data.timestamps.map(t => 
                        new Date(t).toLocaleTimeString()
                    );
                    windChart.data.datasets[0].data = data.speed_ms;
                    windChart.data.datasets[1].data = data.frequency;
                    windChart.update('none');
                }
                
            } catch (error) {
                console.error('Failed to load history:', error);
            }
        }
        
        // Setup Server-Sent Events for real-time updates
        function setupEventSource() {
            const evtSource = new EventSource('/api/events');
            
            evtSource.onmessage = function(event) {
                const data = JSON.parse(event.data);

                document.getElementById('speed-ms').textContent = data.speed_ms.toFixed(3);
                document.getElementById('speed-kmh').textContent = data.speed_kmh.toFixed(3);
                document.getElementById('speed-knots').textContent = data.speed_knots.toFixed(3);
                document.getElementById('frequency').textContent = data.frequency.toFixed(3);
                document.getElementById('last-update').textContent =
                    'Last update: ' + new Date().toLocaleTimeString();
                
                // Update chart
                windChart.data.labels.push(new Date().toLocaleTimeString());
                windChart.data.datasets[0].data.push(data.speed_ms);
                windChart.data.datasets[1].data.push(data.frequency);
                
                // Keep only last 60 points
                if (windChart.data.labels.length > 60) {
                    windChart.data.labels.shift();
                    windChart.data.datasets[0].data.shift();
                    windChart.data.datasets[1].data.shift();
                }
                
                windChart.update('none');
                updateStatus(true, 'Live');
            };
            
            evtSource.onerror = function() {
                updateStatus(false, 'Disconnected');
                setTimeout(setupEventSource, 5000);
            };
        }
        
        // Initialize
        loadSensors();
        loadHistory();
        setupEventSource();
        
        // Refresh every 5 seconds as fallback
        setInterval(refreshData, 5000);
    </script>
</body>
</html>
'''

STYLE_CSS = '''/* Additional styles if needed */'''


def main():
    """Main entry point."""
    parser = argparse.ArgumentParser(
        description="Anemometer Wind Data Web Server",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s                           # Start server on port 8080
  %(prog)s --port 3000               # Start server on port 3000
  %(prog)s --sensor outdoor          # Monitor specific sensor
  %(prog)s --interval 2.0            # Update every 2 seconds
        """
    )
    
    parser.add_argument("--port", "-p", type=int, default=DEFAULT_PORT,
                       help=f"Server port (default: {DEFAULT_PORT})")
    parser.add_argument("--sensor", "-s", default=DEFAULT_SENSOR,
                       help="Sensor name (auto-detect if not specified)")
    parser.add_argument("--interval", "-i", type=float, default=1.0,
                       help="Data collection interval in seconds (default: 1.0)")
    
    args = parser.parse_args()
    
    # Check if driver is loaded
    sensors = get_sensors()
    if not sensors:
        print("Error: No anemometer sensors found!", file=sys.stderr)
        print("Make sure the anemometer driver is loaded:", file=sys.stderr)
        print("  sudo modprobe anemometer", file=sys.stderr)
        sys.exit(1)
    
    # Auto-detect sensor if not specified
    sensor_name = args.sensor
    if not sensor_name:
        sensor_name = sensors[0]
        print(f"Auto-detected sensor: {sensor_name}")
    elif sensor_name not in sensors:
        print(f"Error: Sensor '{sensor_name}' not found!", file=sys.stderr)
        print(f"Available sensors: {', '.join(sensors)}", file=sys.stderr)
        sys.exit(1)
    
    # Start data collector thread
    collector = threading.Thread(
        target=data_collector, 
        args=(sensor_name, args.interval),
        daemon=True
    )
    collector.start()
    
    # Start HTTP server
    server = HTTPServer(("0.0.0.0", args.port), AnemometerHandler)
    
    print(f"\n{'='*60}")
    print(f"🌬️  Anemometer Web Server")
    print(f"{'='*60}")
    print(f"\nSensor: {sensor_name}")
    print(f"URL: http://localhost:{args.port}")
    print(f"API: http://localhost:{args.port}/api/data")
    print(f"\nPress Ctrl+C to stop")
    print(f"{'='*60}\n")
    
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\n\nShutting down server...")
        server.shutdown()
        print("Goodbye!")


if __name__ == "__main__":
    main()
