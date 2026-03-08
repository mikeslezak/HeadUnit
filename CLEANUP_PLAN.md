# HeadUnit Cleanup Plan

## Files to Remove (Move to archive/)

### Temporary Bluetooth Troubleshooting Scripts
These were used for debugging and are no longer needed:
- `enable_bluetooth_battery.sh` - Old battery enable attempt
- `fix_bluetooth_battery.sh` - Old battery fix attempt
- `fix_bluetooth_config.sh` - Old config fix
- `fix_bluetooth_nvidia_override.sh` - Fixed NVIDIA conflict (done)
- `pair_iphone.sh` - Manual pairing script (debug only)
- `setup_pairing.sh` - Manual pairing script (debug only)
- `unpair_and_repair_iphone.sh` - Manual pairing script (debug only)

### One-Time Setup Scripts
These were used once and are complete:
- `upgrade_bluez.sh` - Upgraded to BlueZ 5.71 (completed)
- `switch_to_new_bluez.sh` - Switched to new binary (completed)

## Files to Keep

### Active Scripts
- `launch_headunit.sh` - Main app launcher (KEEP)
- `install_power_monitor.sh` - Future truck installation (KEEP)
- `power_monitor.py` - Power management (KEEP)
- `power-monitor.service` - Systemd service (KEEP)

### Documentation
- `POWER_SETUP.md` - Power system docs (KEEP)
- `CLEANUP_PLAN.md` - This file (KEEP)

## Code Changes Needed

### BluetoothManager.cpp
**Remove/Disable:**
1. Battery1 polling timer (lines 193-196)
   - Doesn't work for iPhones
   - Spams error logs every 30 seconds

2. `checkBatteryLevels()` function (lines 1357-1371)
   - Never successfully reads battery for iPhone

3. `updateDeviceBattery()` function (lines 1374-1422)
   - Battery1 interface doesn't exist for iPhone

4. `getBatteryProperties()` function (lines 547-570)
   - Returns empty for iPhone

5. Battery history tracking (`m_batteryHistory`)
   - Not used since Battery1 doesn't work

6. Charging detection code
   - Depends on Battery1 data

**Keep:**
- Cellular signal monitoring (works)
- Device management
- Connection handling

### NotificationManager.cpp
**Keep Everything:**
- BLE battery reading (correct approach for iPhone)
- Battery refresh timer (just added, needed)
- ANCS notification handling

### StatusBar.qml
**Current Status: Good**
- Uses NotificationManager for battery (correct)
- Uses BluetoothManager for signal (correct)
- Charging status won't work (Battery1 unavailable) - can disable this

## Proposed Actions

1. **Create archive directory**
   ```bash
   mkdir -p /home/mike/HeadUnit/archive/bluetooth-debug
   ```

2. **Move debug scripts to archive**
   - Keeps them for reference
   - Cleans up main directory

3. **Disable Battery1 polling code**
   - Comment out timer initialization
   - Add note explaining why

4. **Remove charging indicator from UI** (optional)
   - Since Battery1 unavailable, charging detection doesn't work
   - Could implement via BLE later if needed

5. **Test after cleanup**
   - Verify HeadUnit still launches
   - Verify battery reading works (from NotificationManager)
   - Verify cellular signal works

## What This Achieves

- **Cleaner codebase**: Remove ~400 lines of dead code
- **Better logs**: No more "Battery1 interface not found" spam
- **Clearer architecture**: One battery source (BLE), not two conflicting ones
- **Easier maintenance**: Less code to maintain
- **Better performance**: No wasted CPU checking unavailable interfaces

## Rollback Plan

If anything breaks:
1. All scripts backed up in `archive/`
2. Git history preserved
3. Can restore Battery1 code if needed (though it doesn't work anyway)
