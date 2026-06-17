# SecureInferNode

**Attested TinyML inference on STM32F407 + ESP32** — ECDSA P-256 model signing, SPDM 1.2 remote attestation, anti-rollback enforcement, and MPU-isolated inference, all built on Zephyr RTOS and verified on real hardware.

## Why this project exists

Edge ML inference nodes typically run unsigned model binaries. A tampered anomaly-detection model can silently output "OK" forever — disabling safety-critical detection with zero OS-level error, and no way for an operator to know the device is compromised.

This project applies platform-security techniques (secure boot, remote attestation, anti-rollback) — normally seen in server/mobile root-of-trust designs — to a constrained Cortex-M4 running a TinyML model.

## What's verified on hardware (not simulated)

- **ECDSA P-256 boot verification** (mbedTLS) — the STM32 computes SHA-256 over the model binary and verifies its signature before allowing inference to start. A single flipped byte triggers `k_panic()` and halts the system.
- **SPDM 1.2 GET_MEASUREMENTS** — the device reports SHA-384 of the running model over UART; this hash matches `sha384sum` of the signed `.tflite` file byte-for-byte, closing the remote-attestation loop.
- **Anti-rollback** — the device rejects any model version less than or equal to the last accepted version.
- **ESP32 OTA gateway** — connects to WiFi, fetches the signed model over HTTP, independently re-verifies the ECDSA signature, and forwards it to the STM32 — a two-gate verification design that isolates the WiFi attack surface from the trusted compute core.
- **K_USER MPU-isolated inference thread** — the TFLite Micro inference loop runs unprivileged on the Cortex-M4.

## Documentation

- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) — system design, memory layout, performance numbers
- [docs/THREAT_MODEL.md](docs/THREAT_MODEL.md) — STRIDE analysis, security control status, and a documented hardware-debugging session (register-level pinctrl analysis via OpenOCD)
- [docs/DEMO_OUTPUT.md](docs/DEMO_OUTPUT.md) — real UART captures: valid boot vs. tampered-model rejection

---

## Hardware

| Board | Role |
|---|---|
| STM32F407VG Discovery | Inference node (Cortex-M4, 1MB Flash, 192KB RAM) |
| ESP32 DevKit | WiFi OTA gateway |

## Quick Start

```bash
# ── 1. Clone + set up Zephyr workspace ──────────────────────────────────
git clone https://github.com/such88/SecureNodeML.git SecureInferNode
cd SecureInferNode
west init -l .
west update             # downloads Zephyr v3.7.0 + modules (~1GB, 10-20 min)

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
west build -p auto -b stm32f4_disco firmware/stm32

# ── 8. Flash (WSL2, after usbipd attach) ─────────────────────────────────
west flash --runner openocd --build-dir firmware/stm32/build

# ── 9. Open UART console (USB-TTL on PA2/PA3, 115200 8N1) ─────────────────
sudo stty -F /dev/ttyUSB0 115200 raw -echo && picocom -b 115200 /dev/ttyUSB0
```

## Zephyr Setup (do once)

```bash
# In WSL2 (Ubuntu 22.04)
sudo apt update && sudo apt install -y git cmake ninja-build gperf \
  ccache dfu-util wget python3-pip python3-venv libsdl2-dev usbutils

pip3 install --user west
echo 'export PATH=$HOME/.local/bin:$PATH' >> ~/.bashrc && source ~/.bashrc

# Zephyr SDK (ARM toolchain)
cd ~
wget https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v0.16.8/zephyr-sdk-0.16.8_linux-x86_64_minimal.tar.xz
tar xf zephyr-sdk-0.16.8_linux-x86_64_minimal.tar.xz
cd zephyr-sdk-0.16.8 && ./setup.sh -t arm-zephyr-eabi

# Verify
arm-zephyr-eabi-gcc --version
```

## Build Commands Reference

```bash
# Build STM32 firmware
west build -p auto -b stm32f4_disco firmware/stm32

# Clean build
west build -p always -b stm32f4_disco firmware/stm32

# Flash via openocd (after usbipd attach on Windows)
west flash --runner openocd --build-dir firmware/stm32/build

# Build + flash ESP32 (from firmware/esp32/)
cd firmware/esp32
. ~/esp-idf/export.sh
idf.py set-target esp32
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

## Build Progress

| Day | Milestone | Status |
|---|---|---|
| 1–2 | Zephyr toolchain + blinky | done |
| 3–4 | Sine model + TFLite convert | done |
| 8 | Anomaly autoencoder (102x detection ratio) | done |
| 10 | ECDSA signing pipeline | done |
| 11 | Boot-time ECDSA verify + tamper test | done |
| 12 | K_USER inference thread | done |
| 13–14 | ARCHITECTURE.md + THREAT_MODEL.md | done |
| 15–16 | SPDM GET_MEASUREMENTS — SHA-384 attestation verified | done |
| 17 | Anti-rollback version check | done |
| 19–20 | ESP32 WiFi + HTTP fetch + ECDSA verify | done |
| 21–22 | UART OTA delivery — 4104 bytes, CRC32 OK, anti-rollback enforced | done |
| 23–24 | Docs finalized | done |
| 25–30 | Resume, LinkedIn, Apply | in progress |
