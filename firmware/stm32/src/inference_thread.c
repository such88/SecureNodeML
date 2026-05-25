/*
 * src/inference_thread.c — TFLite Micro anomaly detection in K_USER mode
 *
 * This thread runs unprivileged (K_USER). The Cortex-M4 MPU:
 *   - Allows READ from model flash region (weights)
 *   - Allows READ+WRITE to tensor_arena only (activations)
 *   - FAULTS on write to model flash → proves model cannot be tampered
 *
 * Anomaly detection:
 *   4 sensor inputs → autoencoder → 4 reconstructed outputs
 *   MSE(input, output) > threshold → ANOMALY alert
 *
 * TODO Day 9: add anomaly_model_data.cc, uncomment TFLite Micro calls
 * TODO Day 12: uncomment MPU fault test
 */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/timing/timing.h>

LOG_MODULE_REGISTER(inference, LOG_LEVEL_INF);

/* Anomaly threshold — tune after deploying on hardware (Day 9) */
#define ANOMALY_THRESHOLD   0.05f
#define NUM_FEATURES        4
#define INFERENCE_PERIOD_MS 100

/*
 * TFLite Micro tensor arena — lives in RAM (K_USER thread's domain)
 * 20KB is enough for 4→8→2→8→4 autoencoder with INT8 weights
 */
#define TENSOR_ARENA_SIZE   (20U * 1024U)
static uint8_t tensor_arena[TENSOR_ARENA_SIZE];
/*
 * TODO Day 9: uncomment after running:
 *   xxd -i models/converted/anomaly_int8.tflite > firmware/stm32/src/anomaly_model_data.cc
 * and uncommenting src/anomaly_model_data.cc in CMakeLists.txt
 */
/* extern const uint8_t anomaly_int8_tflite[];      */
/* extern const unsigned int anomaly_int8_tflite_len; */

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

    LOG_INF("Inference thread running (K_USER, MPU-isolated)");

    (void)tensor_arena;  // Avoid "defined but not used" warning until TFLite code is uncommented
    /*
     * MPU fault test — Day 12:
     * Uncomment to prove the inference thread CANNOT write the model.
     * Should trigger "***** MPU FAULT *****" in UART log.
     *
     * extern const uint8_t anomaly_int8_tflite[];
     * *((uint8_t *)anomaly_int8_tflite) = 0xFF;  // MUST fault here
     */

    /*
     * TODO Day 9: initialise TFLite Micro interpreter
     *
     * const tflite::Model* tfl_model =
     *     tflite::GetModel(anomaly_int8_tflite);
     *
     * tflite::MicroMutableOpResolver<4> resolver;
     * resolver.AddFullyConnected();
     * resolver.AddRelu();
     *
     * tflite::MicroInterpreter interpreter(
     *     tfl_model, resolver, tensor_arena, TENSOR_ARENA_SIZE);
     * interpreter.AllocateTensors();
     */

    while (1) {
        /* Replace with real ADC/I2C sensor reads */
        float sensor[NUM_FEATURES] = {0.50f, 0.30f, 0.80f, 0.60f};
        float recon[NUM_FEATURES]  = {0.0f};

        timing_t t0 = timing_counter_get();

        /*
         * TODO Day 9: run inference
         *
         * float *inp = interpreter.input(0)->data.f;
         * memcpy(inp, sensor, NUM_FEATURES * sizeof(float));
         * TfLiteStatus status = interpreter.Invoke();
         * if (status != kTfLiteOk) {
         *     LOG_ERR("Inference failed");
         *     continue;
         * }
         * float *out = interpreter.output(0)->data.f;
         * memcpy(recon, out, NUM_FEATURES * sizeof(float));
         */

        /* TODO inference */
        timing_t t1 = timing_counter_get();
        uint64_t lat_us = timing_cycles_to_ns(timing_cycles_get(&t0, &t1)) / 1000ULL;

        float err = mse(sensor, recon, NUM_FEATURES);

        LOG_INF("Lat:%lluus  Err:%.4f  [temp=%.2f vib=%.2f volt=%.2f curr=%.2f]  %s",
                lat_us, (double)err,
                (double)sensor[0], (double)sensor[1],
                (double)sensor[2], (double)sensor[3],
                (err > ANOMALY_THRESHOLD) ? "*** ANOMALY ***" : "OK");

        k_msleep(INFERENCE_PERIOD_MS);
    }
}
