/*
 * src/main.c — SecureInferNode boot entry point
 *
 * Boot sequence (runs in privileged mode):
 *   1. Verify model ECDSA signature → k_panic() if invalid
 *   2. Anti-rollback version check  → reject if downgrade
 *   3. Create inference thread in K_USER (MPU-isolated)
 *   4. Start SPDM responder on USART1
 *
 * Build:  west build -p auto -b disco_f407vg firmware/stm32
 * Flash:  west flash --runner openocd
 */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
//#include <zephyr/timing/timing.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/drivers/uart.h>
#include "model_verify.h"
#include "version_check.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

/*
 * Model lives in flash partition labelled "model-storage"
 * defined in boards/disco_f407vg.overlay → sector 7 = 0x08060000
 */
#define MODEL_PARTITION     FIXED_PARTITION_ID(model_partition)
#define MODEL_FLASH_OFFSET  0x00000000   /* start of partition */
#define MODEL_MAX_LEN       (120U * 1024U)
/* Signature stored immediately after model data in same partition */
#define MODEL_SIG_OFFSET    (MODEL_MAX_LEN)
#define MODEL_SIG_LEN       72U           /* max DER ECDSA P-256 sig */

#define MODEL_FLASH_ADDR    0x08060000U
#define MODEL_ACTUAL_LEN    4104U
#define MODEL_SIG_ADDR      0x08061008U
#define MODEL_SIG_LEN       72U

/* Inference thread — declared in inference_thread.c */
extern void inference_thread_entry(void *, void *, void *);

K_THREAD_STACK_DEFINE(inference_stack, 8192);
static struct k_thread inference_thread_data;

int main(void)
{
    LOG_INF("=== SecureInferNode v0.1 ===");
    LOG_INF("Board: disco_f407vg  MCU: STM32F407VG  RTOS: Zephyr");

    /* ── Step 1: Verify model before doing anything else ─────────────── */
    //timing_init();
    //timing_start();
    //timing_t t0 = timing_counter_get();
    uint32_t t0 = k_cycle_get_32();

    /*
     * Model is in flash — access via direct pointer to partition base.
     * FIXED_PARTITION_DEVICE gives us the flash device; we read via
     * flash_read() or direct pointer since flash is memory-mapped on M4.
     *
     * Direct pointer approach (works on STM32 — flash is memory-mapped):
     */
    const uint8_t *model = (const uint8_t *)
        (DT_REG_ADDR(DT_NODELABEL(flash0)) + 0x60000);  /* sector 7 */
    const uint8_t *sig   = model + MODEL_ACTUAL_LEN;
    int ret = 0;

    /* TODO Day 11: re-enable after writing model to flash sector 7, todo */
    ret = model_verify(model, MODEL_ACTUAL_LEN, sig, MODEL_SIG_LEN);

    if (ret != 0) {
        LOG_ERR("MODEL VERIFY FAILED — SYSTEM HALTED");
        k_panic();
    }
    LOG_INF("MODEL VERIFIED OK");

    uint32_t t1 = k_cycle_get_32();
    uint32_t verify_us = k_cyc_to_us_near32(t1 - t0);
    //uint64_t verify_us = timing_cycles_to_ns(
    //    timing_cycles_get(&t0, timing_counter_get())) / 1000ULL;

    if (ret != 0) {
        LOG_ERR("MODEL VERIFY FAILED (%u us) — SYSTEM HALTED", verify_us);
        k_panic();
        /* unreachable */
    }
    LOG_INF("MODEL VERIFIED OK (%u us)", verify_us);

    /* Enable USART1 IRQ early so ring buffer captures all OTA bytes */
    extern const struct device *uart1_dev;
    extern void uart_ota_irq_enable(void);
    uart_ota_irq_enable();
    
    /* ── Step 2: Anti-rollback version check ───────────────────── */
    extern int version_check_and_update(uint32_t incoming);
    extern uint32_t version_get_current(void);
    
    uint32_t model_version = 1;  /* hardcoded for now — Day 21: read from OTA frame */
    int vret = version_check_and_update(model_version);
    if (vret != 0) {
        LOG_ERR("VERSION CHECK FAILED — rollback detected");
        k_panic();
    }
    LOG_INF("Version accepted: v%u", version_get_current());

    /* ── SPDM measurement test (Day 15) ─────────────────── */
    extern int spdm_process_request(const uint8_t *req, size_t req_len,
                                     uint8_t *out_buf, size_t out_max);
    uint8_t spdm_req[4] = {0x12, 0xE0, 0x01, 0xFF};  /* GET_MEASUREMENTS */
    uint8_t spdm_resp[64];
    int n = spdm_process_request(spdm_req, sizeof(spdm_req),
                                  spdm_resp, sizeof(spdm_resp));
    if (n > 0) {
        LOG_INF("SPDM response: %d bytes", n);
        LOG_HEXDUMP_INF(spdm_resp, n, "SPDM:");
    } else {
        LOG_ERR("SPDM failed");
    }
    /* ── Step 2: Anti-rollback ────────────────────────────────────────── */
    /*
     * TODO Day 17: read version from model header at model[0..3]
     * uint32_t incoming_ver = *(uint32_t *)model;
     * if (version_check_and_update(incoming_ver) != 0) { k_panic(); }
     */

    /* ── Step 3: Launch inference thread in unprivileged mode ────────── */
    k_thread_create(&inference_thread_data,
                    inference_stack,
                    K_THREAD_STACK_SIZEOF(inference_stack),
                    inference_thread_entry,
                    NULL, NULL, NULL,
                    K_PRIO_PREEMPT(5),
                    K_USER,
                    K_NO_WAIT);
    k_thread_name_set(&inference_thread_data, "inference");
    LOG_INF("Inference thread started");
    
    /* ── Step 4: SPDM responder + OTA listener ────────────────────────── */
    /* TODO Days 15–22: add spdm_responder_run() and ota_receiver_run() */
/* ── OTA receiver ────────────────────────────────────────── */
    extern int ota_receive_and_apply(void);
    LOG_INF("Waiting for OTA from ESP32...");
    int ota_ret = ota_receive_and_apply();
    if (ota_ret == 0) {
        LOG_INF("OTA complete");
    } else {
        LOG_WRN("OTA not received (%d) — continuing", ota_ret);
    }

    /* Sniff test pattern as receiver */
    // const struct device *u1 = DEVICE_DT_GET(DT_NODELABEL(usart1));
    // if (device_is_ready(u1)) {
    //     LOG_INF("USART1 RX test: listening for 15 seconds...");
    //     uint32_t start = k_uptime_get_32();
    //     int total = 0;
    //     while ((k_uptime_get_32() - start) < 15000) {
    //         uint8_t c;
    //         if (uart_poll_in(u1, &c) == 0) {
    //             LOG_INF("RX byte #%d: 0x%02X", total++, c);
    //         }
    //         k_msleep(5);
    //     }
    //     LOG_INF("USART1 RX test done. Total bytes: %d", total);
    // } else {
    //     LOG_ERR("USART1 not ready");
    // }  

   /* TEMP TEST: send a counter byte on USART1 every 200ms */
    // const struct device *u1 = DEVICE_DT_GET(DT_NODELABEL(usart1));
    // if (device_is_ready(u1)) {
    //     LOG_INF("USART1 TX test: sending counter bytes...");
    //     uint8_t counter = 0;
    //     for (int i = 0; i < 50; i++) {
    //         uart_poll_out(u1, counter);
    //         LOG_INF("Sent: 0x%02X", counter);
    //         counter++;
    //         k_msleep(200);
    //     }
    //     LOG_INF("USART1 TX test done");
    // } else {
    //     LOG_ERR("USART1 not ready");
    // }

    return 0;
}
