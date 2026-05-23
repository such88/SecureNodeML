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
#include <zephyr/timing/timing.h>
#include <zephyr/storage/flash_map.h>

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

/* TODO Day 7: replace MODEL_MAX_LEN with actual length from model header */
#define MODEL_ACTUAL_LEN    MODEL_MAX_LEN

/* Inference thread — declared in inference_thread.c */
extern void inference_thread_entry(void *, void *, void *);

K_THREAD_STACK_DEFINE(inference_stack, 8192);
static struct k_thread inference_thread_data;

int main(void)
{
    LOG_INF("=== SecureInferNode v0.1 ===");
    LOG_INF("Board: disco_f407vg  MCU: STM32F407VG  RTOS: Zephyr");

    /* ── Step 1: Verify model before doing anything else ─────────────── */
    timing_init();
    timing_start();
    timing_t t0 = timing_counter_get();

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

    int ret = model_verify(model, MODEL_ACTUAL_LEN, sig, MODEL_SIG_LEN);

    uint64_t verify_us = timing_cycles_to_ns(
        timing_cycles_get(&t0, timing_counter_get())) / 1000ULL;

    if (ret != 0) {
        LOG_ERR("MODEL VERIFY FAILED (%llu us) — SYSTEM HALTED", verify_us);
        k_panic();
        /* unreachable */
    }
    LOG_INF("MODEL VERIFIED OK (%llu us)", verify_us);

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
                    K_USER,          /* <── MPU-isolated unprivileged */
                    K_NO_WAIT);
    k_thread_name_set(&inference_thread_data, "inference");
    LOG_INF("Inference thread started (K_USER)");

    /* ── Step 4: SPDM responder + OTA listener ────────────────────────── */
    /* TODO Days 15–22: add spdm_responder_run() and ota_receiver_run() */

    return 0;
}
