/*
 * firmware/esp32/main/main.c — SecureInferNode WiFi OTA gateway
 *
 * Build: cd firmware/esp32 && idf.py build
 * Flash: idf.py -p /dev/ttyUSB0 flash monitor
 *
 * Pipeline:
 *  1. Connect to WiFi
 *  2. Fetch model (.tflite) + signature (.sig) over HTTP
 *  3. ECDSA P-256 verify — discard if invalid (first security gate)
 *  4. Send to STM32 over UART2 [MAGIC][VERSION][LENGTH][PAYLOAD][CRC32]
 *  5. SPDM GET_MEASUREMENTS → verify hash matches model sent (attestation)
 *
 * TODO Days 19–23: implement each stage
 */
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_http_client.h"
#include "nvs_flash.h"
#include "driver/uart.h"

#include "gateway_verify.h"
#include "ota_protocol.h"

static const char *TAG = "gateway";

/* ── Configuration — set via idf.py menuconfig or sdkconfig ────────────
 * Run: idf.py menuconfig → Example Configuration → set WiFi + server URL
 */
#define OTA_SERVER      "http://192.168.1.100:8080"   /* your PC IP */
#define MODEL_URL       OTA_SERVER "/anomaly_int8.tflite"
#define SIG_URL         OTA_SERVER "/anomaly_int8.tflite.sig"
#define MODEL_VERSION   1U

/* ── UART2: sends model to STM32 ────────────────────────────────────────
 * GPIO17 = UART2 TX → STM32 USART1 RX (PA10)
 * GPIO16 = UART2 RX ← STM32 USART1 TX (PA9)
 */
#define UART_PORT       UART_NUM_2
#define UART_TX_GPIO    17
#define UART_RX_GPIO    16
#define UART_BAUD       115200

static uint8_t s_model_buf[OTA_MAX_MODEL_SIZE];
static uint8_t s_sig_buf[72];   /* max DER ECDSA P-256 signature */
static size_t  s_model_len = 0;
static size_t  s_sig_len   = 0;

/* TODO Day 19: implement wifi_init_sta() */
/* TODO Day 19: implement fetch_file(url, buf, max_len, out_len) */
/* TODO Day 21: implement uart_send_model(buf, len, version) */
/* TODO Day 22: implement spdm_get_measurements() */

void app_main(void)
{
    ESP_LOGI(TAG, "=== SecureInferNode ESP32 Gateway ===");
    ESP_LOGI(TAG, "ECDSA P-256 verify + UART OTA delivery + SPDM attestation");

    nvs_flash_init();

    /* ── TODO Day 19: WiFi ──────────────────────────────────────────── */
    ESP_LOGI(TAG, "TODO Day 19: wifi_init_sta()");

    /* ── TODO Day 19: Fetch model + signature ───────────────────────── */
    ESP_LOGI(TAG, "TODO Day 19: fetch %s", MODEL_URL);
    ESP_LOGI(TAG, "TODO Day 19: fetch %s", SIG_URL);

    /* ── TODO Day 21: ECDSA verify ──────────────────────────────────── */
    if (s_model_len > 0) {
        int ret = gateway_verify(s_model_buf, s_model_len,
                                  s_sig_buf,   s_sig_len);
        if (ret != 0) {
            ESP_LOGE(TAG, "Gateway verify FAILED — model discarded");
            return;
        }
        ESP_LOGI(TAG, "Gateway verify OK");
    }

    /* ── TODO Day 21: Build OTA frame + send to STM32 ──────────────── */
    ESP_LOGI(TAG, "TODO Day 21: uart_send_model()");

    /* ── TODO Day 22: SPDM attestation loop ─────────────────────────── */
    ESP_LOGI(TAG, "TODO Day 22: spdm_get_measurements()");
}
