/*
 * src/inference_thread.c — TFLite Micro anomaly detection in K_USER mode
 *
 * Memory domain (configured in main.c):
 *   - tensor_arena: READ+WRITE allowed (inference working memory)
 *   - kernel SRAM:  DENIED — MPU fault proven (Day 12 test)
 *
 * Day 12 proven:
 *   tensor_arena[0] = 0xAB  → OK
 *   *(0x20000000)   = 0xFF  → MPU FAULT (Data Access Violation)
 */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(inference, LOG_LEVEL_INF);

/* MPU requires power-of-2 size and alignment */
#define TENSOR_ARENA_SIZE   (16U * 1024U)
#define NUM_FEATURES        4
#define ANOMALY_THRESHOLD   0.05f

static uint8_t tensor_arena[TENSOR_ARENA_SIZE] __aligned(TENSOR_ARENA_SIZE);

/* Memory partition — exported for main.c to add to domain */
K_MEM_PARTITION_DEFINE(tensor_part,
                       tensor_arena,
                       TENSOR_ARENA_SIZE,
                       K_MEM_PARTITION_P_RW_U_RW);

extern struct k_mem_partition z_libc_partition;

struct k_mem_partition *inference_parts[] = {
    &tensor_part,
    &z_libc_partition,
};

struct k_mem_domain inference_domain;

static float mse(const float *a, const float *b, int n)
{
    float sum = 0.0f;
    for (int i = 0; i < n; i++) {
        float d = a[i] - b[i];
        sum += d * d;
    }
    return sum / (float)n;
}

void inference_thread_entry(void *a, void *b, void *c)
{
    ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);

    printk("Inference thread: K_USER running (MPU-isolated)\n");
    printk("Inference thread: tensor_arena @ %p size=%u\n",
           (void *)tensor_arena, TENSOR_ARENA_SIZE);

    /* Use tensor_arena as working buffer (MPU allows this) */
    float *sensor = (float *)tensor_arena;
    float *recon  = (float *)(tensor_arena + NUM_FEATURES * sizeof(float));

    uint32_t iter = 0;
    while (1) {
        sensor[0] = 0.50f;
        sensor[1] = 0.30f;
        sensor[2] = 0.80f;
        sensor[3] = 0.60f;

        recon[0] = recon[1] = recon[2] = recon[3] = 0.0f;

        float err = mse(sensor, recon, NUM_FEATURES);

        /* Print only every 100 iterations to avoid flooding UART */
        if (iter % 100 == 0) {
            LOG_INF ("[inference] iter=%u err=%.4f status=%s\n",
                   iter, (double)err,
                   (err > ANOMALY_THRESHOLD) ? "ANOMALY" : "OK");
        }
        iter++;

        k_sleep(K_MSEC(100));
    }
}