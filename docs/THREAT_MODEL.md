# Threat Model — SecureInferNode

> STRIDE analysis. Scope: single-node prototype STM32F407 + ESP32.

---

## 1. Assets

| Asset | Value | Location |
|---|---|---|
| Anomaly detection model | Safety-critical — tampered model = false negatives = undetected failures | Flash 0x08060000 + OTA channel |
| ECDSA private key | Entire supply chain trust | Developer workstation (prod: HSM) |
| SPDM measurement report | Remote attestation integrity | UART + SPDM session |
| Model version counter | Anti-rollback protection | NVS flash 0x080E0000 |

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
| T1 | Replace model in flash | Tampering | JTAG/SWD reflash | ECDSA verify at boot, k_panic() on fail | `src/model_verify.c` | ✅ Implemented |
| T2 | Poisoned OTA model | Tampering | MITM on HTTP | Gateway verify + device verify | `esp32/main/gateway_verify.c`, `src/ota_receiver.c` | ⬜ Pending |
| T3 | Old-version rollback | Tampering | Malicious server | NVS version check, reject if <= stored | `src/version_check.c` | ⬜ Pending |
| T4 | Spoofed SPDM response | Spoofing | UART MITM | Signed measurement response | `src/spdm_responder.c` | ⬜ Pending |
| T5 | TFLite escape to kernel | EoP | Crafted input | K_USER MPU isolation | `src/inference_thread.c` | ⬜ Pending |
| T6 | UART sniffing | Info Disclosure | Physical probe | OUT OF SCOPE — sealed enclosure assumed | N/A | ✅ Scoped out |

---

## 4. Residual Risks (Acknowledged)

| Risk | Why Not Mitigated | Production Fix |
|---|---|---|
| ECDSA timing side-channel | Requires constant-time mbedTLS | Hardware-evaluated mbedTLS build |
| Flash readback | RDP Level 2 destroys dev access | Enable RDP2 at production |
| ESP32 compromise | Gateway treated as semi-trusted | Secure Boot V2 + flash encryption |
| Private key on laptop | Dev convenience | HSM + key ceremony |

---

## 5. Security Controls Status

| Control | File | Verified |
|---|---|---|
| Model signing | `scripts/signing/sign_model.py` | ✅ Day 10 |
| Boot ECDSA verify | `firmware/stm32/src/model_verify.c` | ✅ Day 11 — "Signature VALID" confirmed |
| MPU K_USER thread | `firmware/stm32/src/inference_thread.c` | ⬜ Day 12 — memory domain config pending |
| SPDM responder | `firmware/stm32/src/spdm_responder.c` | ⬜ Day 16 |
| Anti-rollback | `firmware/stm32/src/version_check.c` | ⬜ Day 17 |
| Gateway verify | `firmware/esp32/main/gateway_verify.c` | ⬜ Day 21 |