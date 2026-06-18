# Debug Issues & Solutions — SecureInferNode

> Real hardware debugging log. Every issue encountered and how it was resolved.
> Useful for anyone reproducing this project on the same hardware.

---

## Environment

| Component | Version |
|---|---|
| Host OS | WSL2 Ubuntu 22.04 on Windows 11 |
| Zephyr RTOS | 3.7.0 |
| Zephyr SDK | 0.17.0 (arm-zephyr-eabi-gcc 12.2.0) |
| ESP-IDF | v5.2 |
| Board | STM32F407VG Discovery MB997E |
| ESP32 | DevKit (CP2102 USB-UART) |

---

## Issue 1 — Wrong board name for Zephyr 3.7.0

**Symptom:** `west build -b disco_f407vg` fails with "Invalid BOARD"

**Root cause:** Board name changed in Zephyr 3.6+ from `disco_f407vg` to `stm32f4_disco`

**Fix:**
```bash
west build -p auto -b stm32f4_disco firmware/stm32
```

---

## Issue 2 — STM32 UART console blank after usbipd reattach

**Symptom:** `picocom` opens but no output after STM32 reset

**Root cause:** Prolific PL2303 USB-UART adapter loses configuration after usbipd detach/reattach cycle

**Fix (every session):**
```bash
sudo stty -F /dev/ttyUSB1 115200 raw -echo && picocom -b 115200 /dev/ttyUSB1
```
Also: always detach/reattach the USB-TTL specifically after WSL2 session resume:
```powershell
usbipd detach --busid X-Y
usbipd attach --wsl --busid X-Y
```

---

## Issue 3 — UART port assignment swaps after reattach

**Symptom:** `/dev/ttyUSB0` and `/dev/ttyUSB1` swap between sessions

**Root cause:** usbipd assigns ports in order of attachment, not by device type

**Fix:** Identify port by unplug test:
```bash
# Unplug USB-TTL physically, then:
ls /dev/ttyUSB*
# Missing port = USB-TTL
```

---

## Issue 4 — ESP-IDF v5.1 pkg_resources error

**Symptom:** `idf.py` fails with "pkg_resources cannot be imported"

**Root cause:** ESP-IDF v5.1 incompatible with newer setuptools (>70) which removed `pkg_resources`

**Fix:** Upgrade to ESP-IDF v5.2:
```bash
cd ~/esp-idf
git checkout v5.2
git submodule update --init --recursive
rm -rf /home/suchlp/.espressif/python_env/idf5.1_py3.10_env
./install.sh esp32
. ./export.sh
```

---

## Issue 5 — Zephyr prj.conf inline comments cause build failure

**Symptom:** `error: Aborting due to Kconfig warnings` — value with comment rejected

**Root cause:** Kconfig does not allow inline comments on value lines

**Bad:**
```conf
CONFIG_LOG_DEFAULT_LEVEL=3  # 0=off 1=err 2=warn 3=inf 4=dbg
```
**Fix:**
```conf
# 0=off 1=err 2=warn 3=inf 4=dbg
CONFIG_LOG_DEFAULT_LEVEL=3
```

---

## Issue 6 — mbedTLS unaligned memory access fault (USAGE FAULT)

**Symptom:** `***** USAGE FAULT ***** Unaligned memory access` during ECDSA verify

**Root cause:** PUBLIC_KEY_X/Y arrays not aligned to 4-byte boundary; mbedTLS MPI reads them with word-aligned instructions

**Fix:** Add `__attribute__((aligned(4)))` to key arrays in `model_verify.c`:
```c
static const uint8_t PUBLIC_KEY_X[32] __attribute__((aligned(4))) = { ... };
static const uint8_t PUBLIC_KEY_Y[32] __attribute__((aligned(4))) = { ... };
```

---

## Issue 7 — USART1 PA9/PA10 not working (AF0 instead of AF7)

**Symptom:** USART1 loopback test fails — `uart_poll_in()` returns -1 consistently

**Diagnosis:** OpenOCD register read showed GPIOA AFRH = `0xA8000000` at reset (AF0 for PA9/PA10)
After firmware boot: AFRH = `0x00000770` (AF7 correct) — pinctrl IS applied
Physical loopback (PA9↔PA10 jumper) still fails — suspected hardware issue on this board revision

**Fix:** Switch USART1 to alternate pins PB6/PB7 (also AF7):
```dts
&usart1 {
    pinctrl-0 = <&usart1_tx_pb6 &usart1_rx_pb7_pullup>;
    pinctrl-names = "default";
    current-speed = <115200>;
    status = "okay";
};
```
PB6/PB7 loopback: 5/5 bytes received with `ret=0` ✅

---

## Issue 8 — ESP32 GPIO16/17 (UART2 default) causes framing errors

**Symptom:** STM32 receives continuous `0x00` bytes with `UART_ERROR_FRAMING` when ESP32 connected

**Root cause:** GPIO16/17 are boot-strapping pins on some ESP32 variants — held LOW during boot, causing break condition on UART RX

**Fix:** Switch ESP32 UART2 to GPIO2 (TX) and GPIO4 (RX):
```c
uart_set_pin(UART_NUM_2, 2, 4, -1, -1);
```

---

## Issue 9 — UART OTA frame byte misalignment (first byte dropped)

**Symptom:** `Bad magic: 0xADC0DE00` — received magic shifted by 1 byte

**Root cause:** First UART byte arrives before interrupt handler is fully registered in ring buffer; hardware FIFO byte gets overwritten before ISR reads it

**Fix:** Add 3-byte training preamble + start marker on ESP32 sender:
```c
uint8_t preamble[4] = {0x55, 0x55, 0x55, 0xAA};
uart_write_bytes(UART_NUM_2, (const char *)preamble, 4);
vTaskDelay(pdMS_TO_TICKS(50));
```
STM32 scans for `0xAA` before reading header. This is equivalent to LIN bus / UART bootloader preamble pattern — production quality.

---

## Issue 10 — NVS settings_subsys_init returns -33 (EDOM)

**Symptom:** `settings_subsys_init: -33` — NVS fails to mount

**Root cause:** NVS requires minimum 2 flash sectors. Original overlay had only 1 sector (128KB). Also: `CONFIG_SETTINGS_NVS_SECTOR_COUNT` not set

**Fix (simplified):** Use RAM-only version counter for prototype:
```c
static uint32_t g_current = 0U;
int version_check_and_update(uint32_t incoming) {
    if (incoming <= g_current) return -EACCES;
    g_current = incoming;
    return 0;
}
```
Production fix: two-sector storage partition + `CONFIG_SETTINGS_NVS_SECTOR_COUNT=2`

---

## Issue 11 — OTA polling UART misses bytes at 115200 baud burst

**Symptom:** `UART read timeout at 2/12 bytes` — partial frame received

**Root cause:** `uart_poll_in()` polling at `k_usleep(100)` intervals (10,000 polls/sec) cannot keep up with 115200 baud burst (~11,520 bytes/sec)

**Fix:** Switch to interrupt-driven UART with ring buffer:
```conf
CONFIG_UART_INTERRUPT_DRIVEN=y
CONFIG_RING_BUFFER=y
```
```c
RING_BUF_DECLARE(rx_ringbuf, 4096);

static void uart_cb(const struct device *dev, void *user_data) {
    uint8_t byte;
    while (uart_irq_update(dev) && uart_irq_rx_ready(dev)) {
        while (uart_fifo_read(dev, &byte, 1) == 1) {
            ring_buf_put(&rx_ringbuf, &byte, 1);
        }
    }
}
```

---

## Issue 12 — mbedTLS ECDSA fails with MBEDTLS_ERR_MPI_ALLOC_FAILED (-0x0010) when CONFIG_USERSPACE=y

**Symptom:** `Signature INVALID: -0x0010` — MPI allocator fails during `mbedtls_ecdsa_read_signature()`

**Root cause:** `CONFIG_USERSPACE=y` changes RAM layout; mbedTLS heap not properly isolated

**Fix:** Add `CONFIG_MBEDTLS_ENABLE_HEAP=y` to `prj.conf`:
```conf
CONFIG_MBEDTLS_ENABLE_HEAP=y
CONFIG_MBEDTLS_HEAP_SIZE=24576
```

---

## Issue 13 — K_USER thread cannot call k_msleep()

**Symptom:** MPU fault when inference thread calls `k_msleep()`

**Root cause:** `k_msleep()` has no syscall handler in Zephyr 3.7.0 — cannot be called from unprivileged K_USER thread

**Fix:** Use `k_sleep(K_MSEC(100))` which has `z_impl_k_sleep` syscall handler:
```bash
grep "z_impl_k_sleep" ~/zephyrproject/zephyr/kernel/sched.c
# → found: has syscall handler, safe for K_USER
```

---

## Issue 14 — K_USER inference thread LOG_INF causes MPU fault

**Symptom:** MPU fault immediately after inference thread starts when using `LOG_INF`

**Root cause:** Zephyr logging subsystem accesses kernel memory structures; K_USER thread without proper memory domain grants cannot access them

**Fix:** Add `z_libc_partition` to inference thread memory domain:
```c
extern struct k_mem_partition z_libc_partition;
struct k_mem_partition *inference_parts[] = {
    &tensor_part,
    &z_libc_partition,
};
k_mem_domain_init(&inference_domain, 2, inference_parts);
```

---

## Issue 15 — MPU partition size must be power of 2

**Symptom:** Build error: "size of the partition must be power of 2"

**Root cause:** Cortex-M4 MPU requires partition size AND start address to be power-of-2 aligned

**Fix:** Change tensor_arena to 16KB with matching alignment:
```c
#define TENSOR_ARENA_SIZE (16U * 1024U)
static uint8_t tensor_arena[TENSOR_ARENA_SIZE] __aligned(TENSOR_ARENA_SIZE);
```

---

## Issue 16 — Windows Firewall blocks ESP32 HTTP fetch

**Symptom:** ESP32 HTTP fetch times out with `select() timeout`

**Fix:**
```powershell
# Allow
netsh advfirewall firewall add rule name="HTTP 8080 all" dir=in action=allow protocol=TCP localport=8080 profile=any

# Remove when done
netsh advfirewall firewall delete rule name="HTTP 8080 all"
```
Also: PC and ESP32 must be on **same network** (both connected to same hotspot/router).

---

## Issue 17 — usbipd bus IDs change between sessions

**Symptom:** Device not found after WSL2 restart

**Fix:** Always run `usbipd list` first to get current bus IDs:
```powershell
usbipd list
usbipd attach --wsl --busid X-Y  # use current ID
```

---

## Quick Reference — Session Startup Checklist

```powershell
# PowerShell (Admin) — run every session
usbipd list
usbipd attach --wsl --busid <ST-LINK>
usbipd attach --wsl --busid <ESP32-CP2102>
usbipd attach --wsl --busid <USB-TTL-Prolific>
netsh advfirewall firewall add rule name="HTTP 8080 all" dir=in action=allow protocol=TCP localport=8080 profile=any
```

```bash
# WSL2 — STM32 console
sudo stty -F /dev/ttyUSB1 115200 raw -echo && picocom -b 115200 /dev/ttyUSB1

# WSL2 — ESP32 monitor
deactivate 2>/dev/null; . ~/esp-idf/export.sh
idf.py -p /dev/ttyUSB0 monitor

# WSL2 — build STM32
cd ~/zephyrproject
west build -p auto -b stm32f4_disco /mnt/d/Suchandan/git/SecureInferNode/firmware/stm32
west flash --runner openocd --build-dir /home/suchlp/zephyrproject/build
```

```powershell
# PowerShell — serve model files for OTA
cd D:\Suchandan\git\SecureInferNode\models\converted
py -3.11 -m http.server 8080 --bind 0.0.0.0
```
