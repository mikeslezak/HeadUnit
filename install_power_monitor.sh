#!/bin/bash
# Install HeadUnit Power Monitor

echo "=========================================="
echo "Installing HeadUnit Power Monitor"
echo "=========================================="
echo ""

# 1. Install Python GPIO library
echo "1. Installing Python GPIO library..."
sudo apt-get update
sudo apt-get install -y python3-rpi.gpio
echo "  ✓ GPIO library installed"
echo ""

# 2. Make power monitor executable
echo "2. Making power monitor executable..."
chmod +x /home/mike/HeadUnit/power_monitor.py
echo "  ✓ Power monitor is executable"
echo ""

# 3. Configure sudo permissions for suspend (no password required)
echo "3. Configuring sudo permissions for suspend..."
echo "mike ALL=(ALL) NOPASSWD: /usr/bin/systemctl suspend" | sudo tee /etc/sudoers.d/headunit-suspend
echo "mike ALL=(ALL) NOPASSWD: /usr/bin/pkill" | sudo tee -a /etc/sudoers.d/headunit-suspend
sudo chmod 0440 /etc/sudoers.d/headunit-suspend
echo "  ✓ Sudo permissions configured"
echo ""

# 4. Install systemd service
echo "4. Installing power monitor service..."
sudo cp /home/mike/HeadUnit/power-monitor.service /etc/systemd/system/
sudo systemctl daemon-reload
echo "  ✓ Service installed"
echo ""

echo "=========================================="
echo "Installation Complete!"
echo "=========================================="
echo ""
echo "Next steps:"
echo ""
echo "1. Wire GPIO:"
echo "   - Connect truck ACC wire (12V) to GPIO4 through voltage divider"
echo "   - Physical pin 7 (GPIO4, BCM numbering)"
echo "   - Use 10kΩ + 4.7kΩ resistors to drop 12V → 3.3V"
echo ""
echo "2. Test the monitor (without auto-start):"
echo "   sudo systemctl start power-monitor"
echo "   sudo journalctl -u power-monitor -f"
echo ""
echo "3. Enable auto-start (after testing):"
echo "   sudo systemctl enable power-monitor"
echo ""
echo "4. To disable:"
echo "   sudo systemctl stop power-monitor"
echo "   sudo systemctl disable power-monitor"
echo ""
