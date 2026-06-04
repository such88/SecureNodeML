# Demo Output — SecureInferNode on STM32F407

Captured from real hardware via USB-TTL on USART2 (PA2/PA3) at 115200 baud.

---


## Scenario 1: Valid signed model boots successfully
*** Booting Zephyr OS build v3.7.0 ***
[00:00:00.000,000] <inf> main: === SecureInferNode v0.1 ===
[00:00:00.000,000] <inf> main: Board: disco_f407vg  MCU: STM32F407VG  RTOS: Zephyr
[00:00:01.585,000] <inf> model_verify: Signature VALID
[00:00:01.585,000] <inf> main: MODEL VERIFIED OK
[00:00:01.585,000] <inf> main: MODEL VERIFIED OK (1585523 us)
[00:00:01.585,000] <dbg> spdm: spdm_process_request: SPDM request: code=0xE0 len=4
[00:00:01.589,000] <inf> spdm: MEASUREMENTS: SHA-384 computed over 4104 bytes at 0x08060000
[00:00:01.589,000] <inf> main: SPDM response: 52 bytes
[00:00:01.589,000] <inf> main: SPDM:
                               12 60 00 01 64 26 85 82  2d 31 b3 26 0d 3d 7c 03 |.`..d&.. -1.&.=|.
                               75 4e b1 99 01 97 ad b7  a8 d4 e1 ec 46 33 55 6d |uN...... ....F3Um
                               c3 d5 47 55 70 45 4a 56  7a 7e bb 38 1f ea 1e 37 |..GUpEJV z~.8...7
                               4e 0c e4 9d                                      |N...
[00:00:01.589,000] <inf> main: Inference thread started

**SHA-384 reported by STM32 SPDM:**
`6426 8582 2d31 b326 0d3d 7c03 754e b199 0197 adb7 a8d4 e1ec 4633 556d c3d5 4755 7045 4a56 7a7e bb38 1fea 1e37 4e0c e49d`

**Verified on PC:**
$ sha384sum models/converted/anomaly_int8.tflite
642685822d31b3260d3d7c03754eb1990197adb7a8d4e1ec4633556dc3d5475570454a567a7ebb381fea1e374e0ce49d

✅ **Hashes match — attestation loop closed.**

---

## Scenario 2: Tampered model rejected

Test: flipped one byte at offset 100 of the model binary in flash.

*** Booting Zephyr OS build v3.7.0 ***
[00:00:00.000,000] <inf> main: === SecureInferNode v0.1 ===
[00:00:00.000,000] <inf> main: Board: disco_f407vg  MCU: STM32F407VG  RTOS: Zephyr
[00:00:00.002,000] <err> model_verify: Signature INVALID: -0x4FE2
[00:00:00.002,000] <err> main: MODEL VERIFY FAILED — SYSTEM HALTED
✅ **Single-byte tampering caught — system refuses to boot. Inference never starts.**

---

## Performance Numbers

| Metric | Value |
|---|---|
| ECDSA P-256 verify | 1585 ms |
| SHA-384 computation | 4 ms |
| Flash used | 52 KB |
| RAM peak | 50 KB |
