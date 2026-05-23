# SecureInferNode

Attested TinyML inference on STM32F407 + ESP32.
SPDM 1.2 · ECDSA P-256 model signing · Zephyr MPU isolation · Secure OTA.

---

## Hardware

| Board | Role |
|---|---|
| STM32F407VG Discovery | Inference node (Cortex-M4, 1MB Flash, 192KB RAM) |
| ESP32 DevKit | WiFi OTA gateway |

## Quick Start

```bash
# ── 1. Clone + set up Zephyr workspace ──────────────────────────────────
git clone https://github.com/YOUR_USERNAME/SecureInferNode.git
cd SecureInferNode
west init -l .          # use this repo as the west manifest
west update             # downloads Zephyr v3.6.0 + modules (~1GB, 10-20 min)

# ── 2. Python environment (Windows native, not WSL2) ────────────────────
pip install -r requirements.txt

# ── 3. Generate signing keys (once only) ─────────────────────────────────
python scripts/signing/keygen.py
# Extract key bytes for firmware:
openssl ec -in keys/public.pem -pubin -noout -text 2>&1
# Copy X and Y into: firmware/stm32/src/model_verify.c
#                    firmware/esp32/main/gateway_verify.c

# ── 4. Train anomaly model ───────────────────────────────────────────────
python models/training/anomaly_train.py

# ── 5. Sign model ────────────────────────────────────────────────────────
python scripts/signing/sign_model.py models/converted/anomaly_int8.tflite 1
python scripts/signing/verify_model.py models/converted/anomaly_int8.tflite

# ── 6. Embed model as C array ────────────────────────────────────────────
xxd -i models/converted/anomaly_int8.tflite > firmware/stm32/src/anomaly_model_data.cc
# Then uncomment src/anomaly_model_data.cc in firmware/stm32/CMakeLists.txt

# ── 7. Build STM32 firmware (in WSL2) ────────────────────────────────────
west build -p auto -b disco_f407vg firmware/stm32

# ── 8. Flash (WSL2, after usbipd attach) ─────────────────────────────────
west flash --runner openocd --build-dir firmware/stm32/build

# ── 9. Open UART terminal ────────────────────────────────────────────────
minicom -D /dev/ttyACM0 -b 115200
```

## Zephyr Setup (Day 1-2 — do once)

```bash
# In WSL2 (Ubuntu 22.04)
sudo apt update && sudo apt install -y git cmake ninja-build gperf \
  ccache dfu-util wget python3-pip python3-venv libsdl2-dev usbutils

pip3 install --user west
echo 'export PATH=$HOME/.local/bin:$PATH' >> ~/.bashrc && source ~/.bashrc

# Zephyr SDK (ARM toolchain)
cd ~
wget https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v0.16.8/\
zephyr-sdk-0.16.8_linux-x86_64_minimal.tar.xz
tar xf zephyr-sdk-0.16.8_linux-x86_64_minimal.tar.xz
cd zephyr-sdk-0.16.8 && ./setup.sh -t arm-zephyr-eabi

# Verify
arm-zephyr-eabi-gcc --version
# Expected: arm-zephyr-eabi-gcc (Zephyr SDK 0.16.8) 12.2.0
```

## Build Commands Reference

```bash
# Build STM32 firmware
west build -p auto -b disco_f407vg firmware/stm32

# Build with verbose output
west build -p auto -b disco_f407vg firmware/stm32 -- -DCMAKE_VERBOSE_MAKEFILE=ON

# Flash via openocd (after usbipd attach on Windows)
west flash --runner openocd --build-dir firmware/stm32/build

# Clean build
west build -p always -b disco_f407vg firmware/stm32

# Build ESP32 (from firmware/esp32/)
cd firmware/esp32
idf.py set-target esp32
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

## Docs

- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)
- [docs/THREAT_MODEL.md](docs/THREAT_MODEL.md)

## Build Progress

| Day | Milestone | Status |
|---|---|---|
| 1–2 | Zephyr toolchain + blinky | ⬜ |
| 3–4 | Sine model + TFLite convert | ⬜ |
| 5–7 | TFLite Micro on STM32F407 | ⬜ |
| 8–9 | Anomaly model deployed + latency logged | ⬜ |
| 10 | ECDSA signing pipeline | ⬜ |
| 11 | Boot-time model verify | ⬜ |
| 12 | MPU inference isolation | ⬜ |
| 13–14 | ARCHITECTURE.md + THREAT_MODEL.md | ⬜ |
| 15–16 | SPDM GET_MEASUREMENTS | ⬜ |
| 17–18 | Anti-rollback + LinkedIn post | ⬜ |
| 19–24 | ESP32 OTA pipeline + attestation | ⬜ |
| 25–30 | Docs · Resume · Blog · Apply | ⬜ |
