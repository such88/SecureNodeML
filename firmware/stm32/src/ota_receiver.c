/*
 * ota_receiver.c — Receive OTA model from ESP32 over USART1 (interrupt-driven)
 */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/sys/crc.h>
#include <string.h>
#include "ota_protocol.h"
#include "model_verify.h"
#include "version_check.h"

LOG_MODULE_REGISTER(ota_rx, LOG_LEVEL_INF);

RING_BUF_DECLARE(rx_ringbuf, 4096);

static void uart_cb(const struct device *dev, void *user_data)
{
    uint8_t byte;
    while (uart_irq_update(dev) && uart_irq_rx_ready(dev)) {
        while (uart_fifo_read(dev, &byte, 1) == 1) {
            ring_buf_put(&rx_ringbuf, &byte, 1);
        }
    }
}

static int ring_read_exact(uint8_t *buf, size_t len, uint32_t timeout_ms)
{
    size_t received = 0;
    uint32_t start = k_uptime_get_32();

    while (received < len) {
        uint32_t got = ring_buf_get(&rx_ringbuf, buf + received, len - received);
        received += got;
        if (received < len) {
            if ((k_uptime_get_32() - start) > timeout_ms) {
                LOG_ERR("Ring read timeout at %u/%u bytes",
                        (unsigned)received, (unsigned)len);
                return -ETIMEDOUT;
            }
            k_msleep(1);
        }
    }
    return 0;
}

const struct device *uart1_dev;

void uart_ota_irq_enable(void)
{
    uart1_dev = DEVICE_DT_GET(DT_NODELABEL(usart1));
    if (!device_is_ready(uart1_dev)) {
        return;
    }
    ring_buf_reset(&rx_ringbuf);
    uart_irq_callback_set(uart1_dev, uart_cb);
    uart_irq_rx_enable(uart1_dev);
}

static uint8_t s_model_buf[5120];

int ota_receive_and_apply(void)
{
    if (!device_is_ready(uart1_dev)) {
        LOG_ERR("USART1 not ready");
        return -ENODEV;
    }

    //uart_irq_rx_enable(uart1_dev);
    LOG_INF("Waiting for OTA frame...");

    /* Scan for start-of-frame marker 0xAA (preceded by 0x55 training bytes) */
    uint8_t b = 0;
    uint32_t t0 = k_uptime_get_32();
    while (b != 0xAA) {
        if (k_uptime_get_32() - t0 > 30000) {
            uart_irq_rx_disable(uart1_dev);
            return -ETIMEDOUT;
        }
        ring_read_exact(&b, 1, 30000);
    }
    LOG_INF("Frame start detected");

    /* No sync byte needed — IRQ was enabled at boot */
    ota_frame_header_t hdr;
    if (ring_read_exact((uint8_t *)&hdr, sizeof(hdr), 30000) != 0) {
        return -ETIMEDOUT;
    }
    
    if (hdr.magic != OTA_MAGIC) {
        LOG_ERR("Bad magic: 0x%08X", hdr.magic);
        uart_irq_rx_disable(uart1_dev);
        return -EINVAL;
    }
    if (hdr.length == 0 || hdr.length > sizeof(s_model_buf)) {
        LOG_ERR("Bad length: %u", hdr.length);
        uart_irq_rx_disable(uart1_dev);
        return -EINVAL;
    }
    LOG_INF("OTA header: v%u len=%u", hdr.version, hdr.length);

    if (ring_read_exact(s_model_buf, hdr.length, 10000) != 0) {
        uart_irq_rx_disable(uart1_dev);
        return -ETIMEDOUT;
    }

    uint32_t recv_crc;
    if (ring_read_exact((uint8_t *)&recv_crc, 4, 1000) != 0) {
        uart_irq_rx_disable(uart1_dev);
        return -ETIMEDOUT;
    }

    uart_irq_rx_disable(uart1_dev);

    uint32_t calc_crc = crc32_ieee(s_model_buf, hdr.length);
    if (calc_crc != recv_crc) {
        LOG_ERR("CRC32 mismatch: got 0x%08X expected 0x%08X",
                recv_crc, calc_crc);
        return -EIO;
    }
    LOG_INF("CRC32 OK");

    int ret = version_check_and_update(hdr.version);
    if (ret != 0) return ret;

    LOG_INF("OTA v%u accepted — %u bytes received", hdr.version, hdr.length);
    return 0;
}
