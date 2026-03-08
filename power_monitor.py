#!/usr/bin/env python3
"""
HeadUnit Power Monitor
Monitors accessory power (ignition) and manages sleep/wake
"""

import RPi.GPIO as GPIO
import time
import subprocess
import os
import signal
import sys

# Configuration
ACC_GPIO_PIN = 4  # GPIO4 (Physical pin 7)
CHECK_INTERVAL = 2  # Check every 2 seconds
SHUTDOWN_DELAY = 30  # Wait 30 seconds after ACC off before sleeping

# State
acc_off_time = None
is_sleeping = False

def setup_gpio():
    """Initialize GPIO pin for accessory power detection"""
    GPIO.setmode(GPIO.BCM)
    GPIO.setup(ACC_GPIO_PIN, GPIO.IN, pull_up_down=GPIO.PUD_DOWN)
    print(f"GPIO {ACC_GPIO_PIN} configured for accessory power detection")

def is_acc_on():
    """Check if accessory power is on (key in ignition)"""
    return GPIO.input(ACC_GPIO_PIN) == GPIO.HIGH

def suspend_system():
    """Put system into suspend (sleep) mode"""
    global is_sleeping
    print("⚡ Accessory power OFF - Suspending system...")
    is_sleeping = True

    # Stop HeadUnit if running
    subprocess.run(['pkill', '-f', 'appHeadUnit'], check=False)

    # Suspend system
    subprocess.run(['sudo', 'systemctl', 'suspend'], check=True)

    # System will resume here when ACC power returns
    is_sleeping = False
    print("⚡ System woke up - Accessory power detected")

def start_headunit():
    """Start the HeadUnit application"""
    print("🚗 Starting HeadUnit...")
    subprocess.Popen(
        ['bash', '/home/mike/HeadUnit/launch_headunit.sh'],
        cwd='/home/mike/HeadUnit/build',
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL
    )

def signal_handler(sig, frame):
    """Clean shutdown on SIGINT/SIGTERM"""
    print("\n⚠️  Power monitor shutting down...")
    GPIO.cleanup()
    sys.exit(0)

def main():
    """Main power monitoring loop"""
    global acc_off_time

    print("=" * 60)
    print("HeadUnit Power Monitor Started")
    print("=" * 60)
    print(f"Monitoring GPIO {ACC_GPIO_PIN} for accessory power")
    print(f"Shutdown delay: {SHUTDOWN_DELAY} seconds")
    print()

    # Setup signal handlers
    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)

    # Setup GPIO
    setup_gpio()

    # Initial state check
    if is_acc_on():
        print("✓ Accessory power is ON")
        acc_off_time = None
        # Start HeadUnit if not already running
        time.sleep(5)  # Wait for system to stabilize
        start_headunit()
    else:
        print("✗ Accessory power is OFF")
        acc_off_time = time.time()

    # Main monitoring loop
    try:
        while True:
            acc_state = is_acc_on()

            if acc_state:
                # Accessory power is ON
                if acc_off_time is not None:
                    # Power just came back on
                    print("✓ Accessory power restored")
                    acc_off_time = None

                    # Check if HeadUnit is running
                    result = subprocess.run(
                        ['pgrep', '-f', 'appHeadUnit'],
                        capture_output=True
                    )
                    if result.returncode != 0:
                        # HeadUnit not running, start it
                        start_headunit()
            else:
                # Accessory power is OFF
                if acc_off_time is None:
                    # Power just went off
                    print("✗ Accessory power lost")
                    acc_off_time = time.time()
                else:
                    # Check if we should suspend
                    time_off = time.time() - acc_off_time
                    if time_off >= SHUTDOWN_DELAY and not is_sleeping:
                        print(f"⏱️  ACC off for {SHUTDOWN_DELAY}s - initiating suspend")
                        suspend_system()
                        # After waking, reset timer
                        acc_off_time = None
                        # Start HeadUnit after wake
                        if is_acc_on():
                            time.sleep(2)
                            start_headunit()

            time.sleep(CHECK_INTERVAL)

    except Exception as e:
        print(f"❌ Error: {e}")
        GPIO.cleanup()
        sys.exit(1)

if __name__ == "__main__":
    main()
