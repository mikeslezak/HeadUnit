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
- **Layer**: `meta-headunit` in `~/yocto/sources/meta-headunit/`
- **Phase 1 COMPLETE**: Minimal image (systemd, NetworkManager, BlueZ, CAN, dropbear) built and flashed
- **Phase 2 NEXT**: Add Qt 6.8, PipeWire 1.6, GStreamer, EGLFS

## Key Technical Decisions
- Qt 6.8 LTS (supported until 2029)
- PipeWire 1.6.2 + WirePlumber 0.5.13 (custom recipes, no PulseAudio, no oFono)
- BlueZ 5.86 (custom recipe) — D-Bus API directly from Qt/C++, no Blueman
- MapLibre Native Qt for maps (replaces QtWebEngine)
- Valhalla for offline routing
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
