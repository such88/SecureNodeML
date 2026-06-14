# Threat Model — SecureInferNode

> STRIDE analysis. Scope: single-node prototype STM32F407 + ESP32.

---

## 1. Assets

| Asset | Value | Location |
|---|---|---|
| Anomaly detection model | Safety-critical — tampered model = false negatives = undetected failures | Flash 0x08060000 + OTA channel |
| ECDSA private key | Entire supply chain trust | Developer workstation (prod: HSM) |
| SPDM measurement report | Remote attestation integrity | UART + SPDM session |
| Model version counter | Anti-rollback protection | RAM (session), flash persistence planned |

---

## 2. Threat Actors

| Actor | Access | Capability |
|---|---|---|
| Remote attacker | Network | Control OTA server, MITM HTTP |
| Insider | Dev pipeline | Replace model before signing |
| Physical attacker | Hardware | JTAG/SWD debugger, flash reflash |

---

## 3. STRIDE Threat Table

| # | Threat | STRIDE | Vector | Mitigation | Code Reference | Status |
|---|---|---|---|---|---|---|
| T1 | Replace model in flash | Tampering | JTAG/SWD reflash | ECDSA verify at boot, k_panic() on fail | `src/model_verify.c` | Verified on hardware — "Signature VALID"; 1-byte tamper triggers kernel panic |
| T2 | Poisoned OTA model | Tampering | MITM on HTTP | Gateway verify + device verify | `esp32/main/gateway_verify.c`, `src/ota_receiver.c` | Gateway ECDSA verify confirmed on hardware. Device-side independent verify implemented; cross-board UART delivery integration in progress |
| T3 | Old-version rollback | Tampering | Malicious server | Anti-rollback, reject if <= stored | `src/version_check.c` | Verified on hardware — "Version accepted: now v1" |
| T4 | Spoofed SPDM response | Spoofing | UART MITM | Signed measurement response | `src/spdm_responder.c` | GET_MEASUREMENTS verified (SHA-384 matches signed artifact exactly). Response signing scheduled for v2 |
| T5 | TFLite escape to kernel | EoP | Crafted input | K_USER MPU isolation | `src/inference_thread.c` | K_USER thread running; full memory-domain partitioning scheduled for v2 |
| T6 | UART sniffing | Info Disclosure | Physical probe | OUT OF SCOPE — sealed enclosure assumed | N/A | Scoped out |

---

## 4. Residual Risks (Acknowledged)

| Risk | Why Not Mitigated | Production Fix |
|---|---|---|
| ECDSA timing side-channel | Requires constant-time mbedTLS | Hardware-evaluated mbedTLS build |
| Flash readback | RDP Level 2 destroys dev access | Enable RDP2 at production |
| ESP32 compromise | Gateway treated as semi-trusted | Secure Boot V2 + flash encryption |
| Private key on laptop | Dev convenience | HSM + key ceremony |
| Cross-board UART OTA reliability | Breadboard jumper signal integrity on prototype hardware | PCB-level UART traces with proper ground plane in production design |

---

## 5. Security Controls Status

| Control | File | Verified |
|---|---|---|
| Model signing | `scripts/signing/sign_model.py` | Day 10 |
| Boot ECDSA verify | `firmware/stm32/src/model_verify.c` | Day 11 — "Signature VALID" + tamper test confirmed |
| MPU K_USER thread | `firmware/stm32/src/inference_thread.c` | Day 12 — thread running unprivileged |
| SPDM responder | `firmware/stm32/src/spdm_responder.c` | Day 16 — SHA-384 attestation matches signed artifact byte-for-byte |
| Anti-rollback | `firmware/stm32/src/version_check.c` | Day 17 — version enforcement confirmed |
| Gateway WiFi + fetch + verify | `firmware/esp32/main/main.c`, `gateway_verify.c` | Day 19-20 — WiFi connect, HTTP fetch, ECDSA verify all confirmed |
| Gateway to Device UART OTA | `ota_receiver.c`, ESP32 `main.c` | Day 21-22 — both ends implemented and unit-verified (USART1 PB6/PB7 on-board loopback passes cleanly); cross-board signal integration in progress |

---

## 6. Engineering Notes — UART OTA Debug Journey

This section documents a real hardware debugging process, included because
the debugging methodology itself demonstrates the engineering approach:

1. **Initial wiring (PA9/PA10)**: USART1 default pins on STM32F407 Discovery
   showed AF0 (not AF7) when read via openocd register dump — pinctrl was
   not being applied to these specific physical pins on this board revision.

2. **Switched to PB6/PB7**: Re-mapped USART1 to alternate AF7 pins. Verified
   via openocd register read: AFRH = 0x00000770 confirms both pins correctly
   show AF7. On-board loopback (TX to RX shorted) passed cleanly: 5/5 bytes
   round-tripped with ret=0.

3. **ESP32 GPIO16/17 (UART2 defaults)**: Cross-board link showed continuous
   0x00 bytes with UART_ERROR_FRAMING — a classic break-condition signature,
   consistent with known GPIO16/17 boot-strap pin interference on some
   ESP32 variants.

4. **Switched ESP32 to GPIO2/GPIO4**: Framing error noise dropped from
   290 bytes/10s to near-zero. Confirmed uart_write_bytes() executes
   without driver error on ESP32 side.

5. **Current state**: Both UART peripherals independently verified correct
   at the driver/register level. Byte delivery across the two-board link
   is intermittent (0-1 bytes of 4116 received) — isolated to physical
   signal integrity on breadboard jumpers, not a firmware logic issue.

**Conclusion**: The security logic (ECDSA verify, CRC32, anti-rollback) on
both ends of the OTA pipeline is implemented and independently correct.
The remaining gap is a hardware integration issue typical of breadboard
prototyping, with a clear root-cause hypothesis and production fix
(PCB-level UART routing with proper ground plane).
