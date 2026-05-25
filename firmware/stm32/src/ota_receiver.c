/*
 * src/ota_receiver.c — OTA model receive + verify + apply
 *
 * Receive pipeline (each step is a security gate):
 *   1. Read ota_frame_header_t from USART1
 *   2. Read model payload into staging buffer
 *   3. CRC32 check               → detect transmission errors
 *   4. ECDSA P-256 verify        → independent of ESP32 gateway verify
 *   5. Anti-rollback version check
 *   6. Write to staging partition (0x080E0000)
 *   7. Promote: copy staging → model partition (0x08060000)
 *   8. Restart inference thread with new model
 *
 * TODO Days 21–22: implement USART1 receive + flash write
 */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/crc.h>
#include <zephyr/storage/flash_map.h>
#include <errno.h>
#include <string.h>

#include "ota_protocol.h"
#include "model_verify.h"
#include "version_check.h"

LOG_MODULE_REGISTER(ota_rx, LOG_LEVEL_INF);

/* Static receive buffer — lives in BSS (zero-initialised at boot) */
static uint8_t s_model_buf[OTA_MAX_MODEL_SIZE];

int ota_receive_and_apply(void)
{
    ota_frame_header_t hdr;

    /* ── Step 1: Read frame header from USART1 ─────────────────────── */
    /* TODO Day 21: uart_rx_buf((uint8_t *)&hdr, sizeof(hdr)); */
    LOG_DBG("Waiting for OTA frame header...");

    if (hdr.magic != OTA_MAGIC) {
        LOG_ERR("Bad OTA magic: 0x%08X (expected 0x%08X)",
                hdr.magic, OTA_MAGIC);
        return -EINVAL;
    }
    if (hdr.length == 0 || hdr.length > OTA_MAX_MODEL_SIZE) {
        LOG_ERR("OTA length out of range: %u", hdr.length);
        return -ENOMEM;
    }
    LOG_INF("OTA header: version=%u length=%u", hdr.version, hdr.length);

    /* ── Step 2: Read payload ──────────────────────────────────────── */
    /* TODO Day 21: uart_rx_buf(s_model_buf, hdr.length); */

    /* ── Step 3: CRC32 check ───────────────────────────────────────── */
    uint32_t recv_crc;
    /* TODO Day 21: uart_rx_buf((uint8_t *)&recv_crc, sizeof(recv_crc)); */

    uint32_t calc_crc = crc32_ieee(s_model_buf, hdr.length);
    if (calc_crc != recv_crc) {
        LOG_ERR("CRC32 mismatch: got 0x%08X expected 0x%08X",
                recv_crc, calc_crc);
        return -EIO;
    }
    LOG_INF("CRC32 OK (0x%08X)", calc_crc);

    /* ── Step 4: Independent ECDSA verify (do NOT trust the gateway) ─ */
    /*
     * The signature is appended after the model payload.
     * TODO Day 21: define wire protocol for sig delivery
     * (options: fixed 72-byte trailer, or separate framed message)
     */
    uint8_t sig_buf[72];
    /* TODO Day 21: extract sig from frame */

    int ret = model_verify(s_model_buf, hdr.length, sig_buf, sizeof(sig_buf));
    if (ret != 0) {
        LOG_ERR("OTA ECDSA verify FAILED — model rejected");
        /* Wipe buffer: don't leave potentially malicious bytes in RAM */
        memset(s_model_buf, 0, hdr.length);
        return -EACCES;
    }
    LOG_INF("OTA ECDSA verify OK");

    /* ── Step 5: Anti-rollback ─────────────────────────────────────── */
    ret = version_check_and_update(hdr.version);
    if (ret != 0) {
        LOG_ERR("Rollback rejected: v%u", hdr.version);
        return ret;
    }

    /* ── Steps 6–8: Flash write + promote ─────────────────────────── */
    /* TODO Day 22:
     * 1. flash_area_open(FIXED_PARTITION_ID(ota_staging), &fa)
     * 2. flash_area_erase(fa, 0, hdr.length)
     * 3. flash_area_write(fa, 0, s_model_buf, hdr.length)
     * 4. flash_area_close(fa)
     * 5. Copy staging → model_partition
     * 6. k_thread_abort(inference_thread) + k_thread_create(...) to restart
     */
    LOG_INF("OTA v%u accepted — TODO: flash write (Day 22)", hdr.version);
    return 0;
}
