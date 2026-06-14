/*
 * ota_receiver.c — Receive OTA model from ESP32 over USART1
 */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/crc.h>
#include <string.h>
#include "ota_protocol.h"
#include "model_verify.h"
#include "version_check.h"

LOG_MODULE_REGISTER(ota_rx, LOG_LEVEL_INF);

static const struct device *uart1 = DEVICE_DT_GET(DT_NODELABEL(usart1));
//RAM OVERFLOW(128KB) — allocate as static global to avoid stack overflow in inference thread
//static uint8_t s_model_buf[OTA_MAX_MODEL_SIZE]; 
static uint8_t s_model_buf[8192];  /* 8KB — enough for our 4104 byte model */

static int uart_read_exact(const struct device *dev,
                            uint8_t *buf, size_t len, uint32_t timeout_ms)
{
    size_t received = 0;
    uint32_t start = k_uptime_get_32();

    while (received < len) {
        uint8_t c;
        if (uart_poll_in(dev, &c) == 0) {
            buf[received++] = c;
        } else {
            if ((k_uptime_get_32() - start) > timeout_ms) {
                LOG_ERR("UART read timeout at %u/%u bytes",
                        (unsigned)received, (unsigned)len);
                return -ETIMEDOUT;
            }
            k_usleep(100);
        }
    }
    return 0;
}

int ota_receive_and_apply(void)
{
    if (!device_is_ready(uart1)) {
        LOG_ERR("USART1 not ready");
        return -ENODEV;
    }

    LOG_INF("Waiting for OTA frame on USART1...");

    /* Step 1: Read header */
    ota_frame_header_t hdr;
    if (uart_read_exact(uart1, (uint8_t *)&hdr, sizeof(hdr), 30000) != 0) {
        return -ETIMEDOUT;
    }

    if (hdr.magic != OTA_MAGIC) {
        LOG_ERR("Bad magic: 0x%08X", hdr.magic);
        return -EINVAL;
    }
    if (hdr.length == 0 || hdr.length > OTA_MAX_MODEL_SIZE) {
        LOG_ERR("Bad length: %u", hdr.length);
        return -EINVAL;
    }
    LOG_INF("OTA header: v%u len=%u", hdr.version, hdr.length);

    /* Step 2: Read payload */
    if (uart_read_exact(uart1, s_model_buf, hdr.length, 10000) != 0) {
        return -ETIMEDOUT;
    }

    /* Step 3: Read + verify CRC32 */
    uint32_t recv_crc;
    if (uart_read_exact(uart1, (uint8_t *)&recv_crc, 4, 1000) != 0) {
        return -ETIMEDOUT;
    }
    uint32_t calc_crc = crc32_ieee(s_model_buf, hdr.length);
    if (calc_crc != recv_crc) {
        LOG_ERR("CRC32 mismatch: got 0x%08X expected 0x%08X",
                recv_crc, calc_crc);
        return -EIO;
    }
    LOG_INF("CRC32 OK");

    /* Step 4: ECDSA verify (independent of gateway) */
    /* TODO Day 22: signature delivered separately — skip for now */
    LOG_INF("ECDSA verify: TODO Day 22");

    /* Step 5: Version check */
    int ret = version_check_and_update(hdr.version);
    if (ret != 0) return ret;

    LOG_INF("OTA v%u accepted — %u bytes received", hdr.version, hdr.length);
    return 0;
}
