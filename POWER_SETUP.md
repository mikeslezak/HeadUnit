# HeadUnit Power Management Setup

## Overview

This setup allows the Jetson HeadUnit to:
- Stay always powered (never fully shuts down)
- Detect when truck ignition is ON/OFF
- Auto-sleep when ignition is OFF (after 30 second delay)
- Wake instantly when ignition turns ON (~2-3 seconds)
- Auto-start HeadUnit app on wake

## Hardware Requirements

### Power Supply
- **12V to 5V/4A Buck Converter** (e.g., DROK buck converter)
  - Input: 12V from truck battery
  - Output: 5V 4A for Jetson

### Voltage Divider Components
To safely read 12V ACC signal with 3.3V GPIO:
- **1x 10kΩ resistor** (R1)
- **1x 4.7kΩ resistor** (R2)
- **Wire** from truck's ACC/IGN wire

## Wiring Diagram

```
TRUCK SIDE                           JETSON SIDE
============                         ===========

Battery (+12V) ─────┬───────────────→ Buck Converter (+5V) ───→ Jetson Power
                    │
                    └──→ [Fuse 5A]

ACC Wire (+12V) ────→ [R1: 10kΩ] ──┬──→ GPIO4 (Pin 7)
  (Key ON/OFF)                      │
                                [R2: 4.7kΩ]
                                    │
                                   GND ───→ Jetson GND (Pin 6)
```

### GPIO Pin Assignments

| Function | GPIO (BCM) | Physical Pin | Direction |
|----------|------------|--------------|-----------|
| ACC Power Detect | GPIO4 | Pin 7 | INPUT |
| Ground | GND | Pin 6 | - |

### Voltage Divider Calculation

```
Vin = 12V (truck ACC)
Vout = 3.3V (Jetson GPIO safe voltage)

Vout = Vin × (R2 / (R1 + R2))
3.3V = 12V × (4.7kΩ / (10kΩ + 4.7kΩ))
3.3V = 12V × 0.32
```

## Software Installation

### 1. Install Power Monitor

```bash
cd /home/mike/HeadUnit
chmod +x install_power_monitor.sh
bash install_power_monitor.sh
```

This will:
- Install Python GPIO library
- Configure sudo permissions
- Install systemd service

### 2. Test Before Enabling

**Test without GPIO (simulated mode):**
```bash
# Start the service manually
sudo systemctl start power-monitor

# Watch the logs
sudo journalctl -u power-monitor -f
```

**Test with GPIO (after wiring):**
```bash
# Check GPIO state
python3 -c "import RPi.GPIO as GPIO; GPIO.setmode(GPIO.BCM); GPIO.setup(4, GPIO.IN); print('ACC:', 'ON' if GPIO.input(4) else 'OFF'); GPIO.cleanup()"

# Start power monitor
sudo systemctl start power-monitor

# Turn key ON/OFF and watch logs
sudo journalctl -u power-monitor -f
```

### 3. Enable Auto-Start (Production)

Once tested and working:
```bash
sudo systemctl enable power-monitor
```

## Configuration

Edit `/home/mike/HeadUnit/power_monitor.py`:

```python
# Pin configuration
ACC_GPIO_PIN = 4  # Physical pin 7

# Timing
CHECK_INTERVAL = 2  # Check every 2 seconds
SHUTDOWN_DELAY = 30  # Wait 30s after ACC off before sleeping
```

## How It Works

### State Machine

```
┌─────────────────────────────────────────────┐
│          SYSTEM RUNNING (Key ON)            │
│  - HeadUnit app running                     │
│  - Monitoring GPIO                          │
└───────────────┬─────────────────────────────┘
                │
                │ KEY TURNS OFF
                │ (GPIO goes LOW)
                ▼
┌─────────────────────────────────────────────┐
│     WAITING TO SLEEP (30 second delay)      │
│  - HeadUnit still running                   │
│  - Counting down                            │
└───────────────┬─────────────────────────────┘
                │
                │ 30 SECONDS ELAPSED
                │
                ▼
┌─────────────────────────────────────────────┐
│            SUSPENDING SYSTEM                │
│  - Stop HeadUnit app                        │
│  - Execute: systemctl suspend               │
│  - System enters low-power sleep            │
└───────────────┬─────────────────────────────┘
                │
                │ KEY TURNS ON
                │ (GPIO interrupt)
                │
                ▼
┌─────────────────────────────────────────────┐
│         WAKING UP (~2-3 seconds)            │
│  - Resume from suspend                      │
│  - Restore system state                     │
└───────────────┬─────────────────────────────┘
                │
                ▼
┌─────────────────────────────────────────────┐
│        AUTO-START HEADUNIT                  │
│  - Launch HeadUnit app                      │
│  - Reconnect Bluetooth                      │
│  - Ready to use!                            │
└─────────────────────────────────────────────┘
```

## Power Consumption

| State | Power Draw | Notes |
|-------|------------|-------|
| Running | ~8-10W | Full operation with display |
| Suspended | ~0.5-1W | RAM powered, CPU sleeping |
| Shutdown | 0W | Completely off (slow boot) |

**Battery drain estimate:**
- Suspended 12 hours: ~6-12Wh (~0.5-1Ah from 12V battery)
- Most car batteries: 50-70Ah capacity
- Days to drain: 50+ days if only suspended

## Troubleshooting

### Power Monitor Not Starting
```bash
# Check service status
sudo systemctl status power-monitor

# Check logs
sudo journalctl -u power-monitor -n 50

# Test GPIO manually
python3 -c "import RPi.GPIO as GPIO; GPIO.setmode(GPIO.BCM); GPIO.setup(4, GPIO.IN, pull_up_down=GPIO.PUD_DOWN); print(GPIO.input(4)); GPIO.cleanup()"
```

### System Not Suspending
```bash
# Test manual suspend
sudo systemctl suspend

# Check if suspend works
sudo journalctl -b | grep suspend
```

### GPIO Not Detecting ACC Power
```bash
# Verify voltage at GPIO pin (should be ~3.3V when key ON)
# Use multimeter to check

# Check voltage divider wiring
# R1 (10k) should be between ACC and GPIO
# R2 (4.7k) should be between GPIO and GND
```

### HeadUnit Not Auto-Starting After Wake
```bash
# Check if launch script works
bash /home/mike/HeadUnit/launch_headunit.sh

# Check for errors in power monitor logs
sudo journalctl -u power-monitor -n 100 | grep -i error
```

## Safety Features

1. **30 Second Delay**: Prevents accidental sleep during brief ignition toggles
2. **Auto-Restart**: Service restarts if it crashes
3. **Clean Shutdown**: HeadUnit is stopped before suspend
4. **GPIO Pull-Down**: Prevents false triggers when ACC wire is disconnected

## Development Mode

To disable auto-sleep while developing:

```bash
# Stop power monitor
sudo systemctl stop power-monitor

# Disable auto-start
sudo systemctl disable power-monitor

# Manually run HeadUnit
bash /home/mike/HeadUnit/launch_headunit.sh
```

## Recovery

If you get locked out:

1. **SSH from another computer**:
   ```bash
   ssh mike@<jetson-ip>
   sudo systemctl stop power-monitor
   ```

2. **Physical console** (if SSH unavailable):
   - Connect keyboard
   - Press Ctrl+Alt+F3 for console
   - Login
   - `sudo systemctl stop power-monitor`

## References

- Jetson GPIO Library: https://github.com/NVIDIA/jetson-gpio
- Systemd Suspend: https://www.freedesktop.org/wiki/Software/systemd/
- Voltage Divider Calculator: https://ohmslawcalculator.com/voltage-divider-calculator
