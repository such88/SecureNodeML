# Architecture — SecureInferNode


> Attested TinyML inference on STM32F407 + ESP32 with SPDM 1.2 attestation
> and ECDSA P-256 model signing.

> **Status:** Core security pipeline verified on real hardware (Days 1-22). Cross-board OTA UART link integration in progress — see THREAT_MODEL.md §6.
> Last updated: 2026-06-13

---

## 1. Problem Statement

Edge ML inference nodes can run tampered models silently. A model that
always outputs "OK" disables safety-critical anomaly detection with no
visible error to the operating system.

This system solves three problems:

1. **Model integrity at rest** — ECDSA P-256 signature verified at boot; `k_panic()` on failure
2. **Model integrity over the air** — signed OTA with independent verification on gateway and device
3. **Remote attestation** — SPDM 1.2 GET_MEASUREMENTS reports SHA-384 of running model to remote verifier

---

## 2. System Diagram

```
┌──────────────────────────────────────────────────────────────┐
│ PC / CI SIGNING PIPELINE                                     │
│                                                              │
│  anomaly_train.py ──► anomaly_int8.tflite                    │
│  sign_model.py    ──► anomaly_int8.tflite.sig (ECDSA P-256) │
│                                                              │
│  Private key: keys/private.pem  ← NEVER committed to git    │
│  Public key:  keys/public.pem   ← committed, embedded in FW │
└──────────────────┬───────────────────────────────────────────┘
                   │ HTTP (dev) / HTTPS (production)
                   ▼
┌──────────────────────────────────────────────────────────────┐
│ ESP32 — UNTRUSTED PERIMETER                                  │
│ (WiFi attack surface stops here)                             │
│                                                              │
│  1. Fetch model(.tflite) + signature(.sig)                   │
│  2. ECDSA P-256 verify (mbedTLS) — discard if invalid        │
│  3. UART Frame: [MAGIC][VERSION][LENGTH][PAYLOAD][CRC32]     │
│  4. Send via UART2 (ESP32 GPIO2/GPIO4 ↔ STM32 PB7/PB6)                                 │
│  5. SPDM GET_MEASUREMENTS (requester)                        │
└──────────────────┬───────────────────────────────────────────┘
                   │ UART 115200 8N1 binary framed
                   ▼
┌──────────────────────────────────────────────────────────────┐
│ STM32F407VG — TRUSTED COMPUTE CORE                           │
│                                                              │
│  BOOT (privileged):                                          │
│  1. ECDSA verify model at 0x08060000 → k_panic() if fail     │
│  2. Anti-rollback: version > NVS stored, else reject         │
│  3. Create inference thread (K_USER, MPU-isolated)           │
│                                                              │
│  RUNTIME — inference thread (unprivileged K_USER):           │
│  4. TFLite Micro: sensor[4] → autoencoder → recon_error      │
│  5. recon_error > threshold → log ANOMALY                    │
│                                                              │
│  RUNTIME — SPDM responder (privileged):                      │
│  6. GET_MEASUREMENTS → SHA-384(model) → signed response      │
│                                                              │
│  OTA receive (privileged, USART1 polling, PB6/PB7):                 │
│  7. CRC32 → ECDSA re-verify → version check → apply          │
└──────────────────────────────────────────────────────────────┘
```
---

## 3. Hardware

| Board | Role | Key specs |
|---|---|---|
| STM32F407VG Discovery | Inference node | Cortex-M4 @ 168MHz, 1MB Flash, 192KB RAM, MB997E rev |
| ESP32 DevKit | OTA gateway | Xtensa LX6 @ 240MHz, WiFi |

---

## 4. Memory Layout — STM32F407VG

```
FLASH 0x08000000 — 0x080FFFFF (1MB)
| Region | Address | Size | Purpose |
|---|---|---|---|
| Bootloader slot | 0x08000000 | 32 KB | Reserved |
| Application firmware | 0x08008000 | 350 KB | Zephyr + mbedTLS + TFLite Micro |
| Model storage | 0x08060000 | 128 KB | anomaly_int8.tflite |
| Signature storage | 0x08061008 | 72 B | ECDSA P-256 DER signature |
| OTA staging | 0x08080000 | 384 KB | Incoming model before verify |
| NVS storage | 0x080E0000 | 128 KB | Anti-rollback version |

SRAM 0x20000000 (128 KB)
  Zephyr kernel + thread stacks
  mbedTLS workspace          ~12 KB
  TFLite Micro tensor arena  ~20 KB
  SPDM message buffers        ~4 KB
  OTA receive buffer          ~8 KB

CCM SRAM 0x10000000 (64 KB) — available for DMA / scratch
```

---

## 5. Trust Boundaries

| Component | Trust | Rationale |
|---|---|---|
| Bootloader | Trusted | Write-protected after RDP |
| Hardcoded public key | Trusted | Immutable after firmware flash |
| Zephyr kernel | Trusted | Configures MPU, assumed uncompromised |
| TFLite Micro inference | Untrusted | Runs K_USER MPU-isolated |
| Model binary | Untrusted until verified | ECDSA verified at boot |
| UART channel | Untrusted | CRC + ECDSA re-verify on receive |
| ESP32 WiFi stack | Untrusted | Gateway isolates WiFi attack surface |

---

## 6. Performance (measured on real hardware)

| Metric | Measured | Notes |
|---|---|---|
| Flash used | 85 KB / 1 MB | 8% — room for SPDM + OTA |
| RAM peak | 64 KB / 192 KB | 33% — mbedTLS heap dominant |
| ECDSA verify latency | 1575 ms | Software mbedTLS, optimisation pending |
| Model size | 4104 bytes | INT8 quantized autoencoder |
| Model SHA-256 | `cb9c253e...059ab2b` | Hash STM32 SPDM will attest |
| Signature size | 72 bytes | DER-encoded ECDSA P-256 |
| Detection ratio | 102× | Anomaly recon error vs normal |

---

## 7. Key Design Decisions

### 7.1 ECDSA P-256 — not RSA-2048

- Key size: 32 B vs 256 B
- Verify time: ~1.5 s vs ~6 s (estimated on this M4)
- mbedTLS flash cost lower
- Decision: P-256 fits the constrained flash and gives acceptable boot latency

### 7.2 SHA-256 for signing, SHA-384 for SPDM measurement
- SHA-256 pairs naturally with P-256 (matching bit security)
- SPDM 1.2 spec (DSP0274 §7.8) requires SHA-384 for measurement digests

### 7.3 INT8 quantization for the model
- 4× smaller (~5KB vs ~20KB float32)
- Detection accuracy preserved: 102× ratio vs 107× float32 baseline
- Signature covers the int8 artifact (signed after quantization)

### 7.4 ESP32 as WiFi gateway, not direct STM32 WiFi
- WiFi attack surface isolated from inference MCU
- Two independent verification gates (gateway + device)

### 7.5 MPU on M4 vs TrustZone-M
- STM32F407 has no TrustZone-M hardware (Cortex-M4)
- Current threat model assumes kernel integrity
- Upgrade path documented: STM32U5 series (Cortex-M33, TrustZone-M)


---


## 8. Future Work

- DICE identity chain (CDI → DeviceID → AliasKey at manufacture)
- TrustZone-M migration to STM32U585
- Post-quantum signatures (ML-DSA-44 replacing ECDSA)
- IETF RATS cloud verifier integration
- NIST SP800-193 Protect/Detect/Recover mapping
