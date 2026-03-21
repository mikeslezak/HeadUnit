# HeadUnit — Claude Project Context

## What This Is
Custom automotive head unit for a 1998 Chevy Silverado with GM LS2 6.0L V8 and custom ECM (6x Teensy 4.1).
Qt 6.8 LTS QML/C++ app running on Jetson Orin Nano 8GB via EGLFS (no desktop environment).

## Architecture
- **Module architecture**: Box-and-wire design. See ~/Desktop/HeadUnit-Module-Architecture.md
- **Each module is sealed** — communicates only through signals/slots/properties
- **main.cpp is the only wiring harness** — all module creation and signal connections happen there
- **Each external system owned by exactly one module** (CAN→VehicleBusManager, BlueZ→BluetoothManager, etc.)
- **QML never makes HTTP calls or opens sockets** — that's C++ module territory

## Foundation Documents (on Jetson Desktop)
- `~/Desktop/HeadUnit-Foundation-Audit.md` — Complete dependency matrix, all decisions, all tracked items
- `~/Desktop/HeadUnit-Module-Architecture.md` — Module boundaries, new modules needed, migration plan
- `~/Desktop/HeadUnit-Yocto-Design.md` — Original Yocto image design (boot sequence, dual-boot)
- `~/Desktop/meta-headunit-layout.md` — Yocto layer structure (6 audit rounds, clean)

## Yocto Build
- **Build server**: Desktop "NightHawk" WSL2 Ubuntu — `ssh -p 2222 mike@192.168.1.215`
- **Never build on Jetson** — NVIDIA flash tools are x86_64-only
- **Layer**: `meta-headunit` in `~/yocto/sources/meta-headunit/` (files exist on both Jetson and desktop, sync via rsync)
- **meta-qt6 branch**: `6.8` (NOT `lts-6.8` — that uses commercial repos)
- **Phase 1 COMPLETE** (2026-03-19): Minimal image (systemd, NetworkManager, BlueZ, CAN, dropbear)
- **Phase 2 COMPLETE** (2026-03-20): Full multimedia/display/nav stack — 9,186 tasks, all succeeded
- **Phase 2 SD card FLASHED** (2026-03-20): Image streamed from desktop to Jetson SD card via dd (30GB, ~45min)
- **EGLFS test app** built natively on Jetson (Ubuntu SSD), deployed to SD card rootfs with auto-start systemd service
- **Awaiting reboot verification** — reboot to SD card to confirm EGLFS display works
- **Phase 3 NEXT**: Picovoice, Whisper+Piper, RAUC, chrony, librespot, gpsd

### Custom Recipes in meta-headunit (5)
| Recipe | Version | Key fixes |
|--------|---------|-----------|
| pipewire_1.6.2.bb | 1.6.2 | `libsystemd` meson rename, new binaries packaged |
| wireplumber_0.5.13.bb | 0.5.13 | branch=master, bash-completion FILES |
| bluez5_5.86.bb | 5.86 | FILESEXTRAPATHS to poky, removed health/sap/ptest |
| qmaplibre-native-qt_3.0.0.bb | 3.0.0 | gitsm://, install path fix, tests removed, -Werror stripped |
| valhalla_3.6.3.bb | 3.6.3 | gitsm:// nobranch+nolfs, protoc cross-compile CMake include |

### SD Card Flash Process
1. `bitbake headunit-image` on desktop (NightHawk WSL2)
2. `cp .../headunit-image-*.ext4 ~/yocto/flash/headunit-image.ext4`
3. `cd ~/yocto/flash && sudo ./make-sdcard -s 32G -b headunit-image signed/flash.xml.tmp headunit-image-phase2.sdcard`
4. On Jetson (booted from NVMe SSD Ubuntu), insert SD card, unmount all partitions
5. Stream: `ssh -p 2222 mike@192.168.1.215 "sudo dd if=.../headunit-image-phase2.sdcard bs=4M" | sudo dd of=/dev/mmcblk0 bs=4M status=progress`
6. NO lz4 piping — truncates at 979MB due to sparse file handling
7. ~45min for 30GB at ~11.6 MB/s over LAN

### Dual-Boot Setup
- **SD card inserted**: Jetson boots to SD card (Yocto HeadUnit image)
- **SD card removed**: Jetson falls back to NVMe SSD (Ubuntu 22.04 dev environment)
- Use SSD boot to flash/modify SD card contents, build test apps, etc.

### Deploying Apps to SD Card Without Full Rebuild
1. Boot Jetson from NVMe SSD (remove SD card, boot, reinsert)
2. Mount SD card rootfs: `sudo mount /dev/mmcblk0p1 /mnt`
3. Build app natively (Ubuntu has g++, install qt6-base-dev qt6-declarative-dev)
4. Copy binary: `sudo cp ./build/app /mnt/usr/bin/`
5. Add systemd service if needed: `sudo cp app.service /mnt/etc/systemd/system/`
6. Enable: `sudo ln -sf /etc/systemd/system/app.service /mnt/etc/systemd/system/multi-user.target.wants/`
7. Unmount and reboot to SD card

## Key Technical Decisions
- Qt 6.8.4 LTS (supported until 2029)
- PipeWire 1.6.2 + WirePlumber 0.5.13 (custom recipes, no PulseAudio, no oFono)
- BlueZ 5.86 (custom recipe) — D-Bus API directly from Qt/C++, no Blueman
- QMapLibre Native Qt 3.0.0 for maps (custom recipe, replaces QtWebEngine)
- Valhalla 3.6.3 for offline routing (custom recipe)
- RAUC for OTA with NVIDIA nvbootctrl A/B slot switching
- All data on NVMe SSD (/data), rootfs on SD card (ext4, UEFI reads ext4 only)
- Gemini Live API prototyping for voice (hybrid with Claude for complex reasoning)

## CAN Bus Safety
- Safety-critical methods (sendTuneDelta, setLaunchControl, setDriveMode) are NOT Q_INVOKABLE
- Two-step confirmation gate: request*() → user confirms → confirmPendingCommand()
- ToolExecutor does NOT expose vehicle commands to AI
- Hardware CAN gateway (7th Teensy) planned between head unit and vehicle bus

## Shared Platform
- `platform/` submodule → github.com/mikeslezak/silverado-platform (private)
- Contains CAN message definitions (VehicleCAN.h, VehicleMessages.h) and types (ModuleIDs.h, VehicleTypes.h)
- Same repo used by ECM and PDCM projects
