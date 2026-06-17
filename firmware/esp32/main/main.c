#include <string.h>
#include <inttypes.h> 
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "nvs_flash.h"
#include "driver/uart.h"
#include "esp_rom_crc.h"
#include "driver/gpio.h"

#include "gateway_verify.h"
#include "ota_protocol.h"

static const char *TAG = "gateway";

#define WIFI_SSID       "Such"
#define WIFI_PASS       "tenida007"
#define WIFI_MAX_RETRY  5

#define OTA_SERVER      "http://192.168.0.6:8080"
#define MODEL_URL       OTA_SERVER "/anomaly_int8.tflite"
#define SIG_URL         OTA_SERVER "/anomaly_int8.tflite.sig"
#define MODEL_VERSION   2U   /* increment when retraining */

static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT  BIT0
#define WIFI_FAIL_BIT       BIT1
static int s_retry_num = 0;

static uint8_t s_model_buf[OTA_MAX_MODEL_SIZE];
static uint8_t s_sig_buf[72];
static size_t  s_model_len = 0;
static size_t  s_sig_len   = 0;

static void wifi_event_handler(void *arg, esp_event_base_t base,
                                int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < WIFI_MAX_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = (ip_event_got_ip_t *)data;
        ESP_LOGI(TAG, "IP: " IPSTR, IP2STR(&e->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static bool wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_event_handler_instance_t i1, i2;
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                         &wifi_event_handler, NULL, &i1);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                         &wifi_event_handler, NULL, &i2);
    wifi_config_t wcfg = { .sta = { .ssid = WIFI_SSID, .password = WIFI_PASS } };
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wcfg);
    esp_wifi_start();
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE,
        pdMS_TO_TICKS(30000));
    return (bits & WIFI_CONNECTED_BIT) != 0;
}

static size_t fetch_file(const char *url, uint8_t *buf, size_t max_len)
{
    size_t total = 0;
    esp_http_client_config_t cfg = { .url = url };
    esp_http_client_handle_t h = esp_http_client_init(&cfg);
    if (esp_http_client_open(h, 0) != ESP_OK) {
        esp_http_client_cleanup(h); return 0;
    }
    esp_http_client_fetch_headers(h);
    int len;
    while ((len = esp_http_client_read(h, (char *)(buf+total),
                                        max_len-total)) > 0) total += len;
    esp_http_client_close(h);
    esp_http_client_cleanup(h);
    ESP_LOGI(TAG, "Fetched %u bytes: %s", (unsigned)total, url);
    return total;
}

static void uart_init(void)
{
    uart_config_t cfg = {
        .baud_rate  = 115200,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
    };
    uart_param_config(UART_NUM_2, &cfg);
    //uart_set_pin(UART_NUM_2, 17, 16, -1, -1); /* default is TX=GPIO17, RX=GPIO16 */
    uart_set_pin(UART_NUM_2, 2, 4, -1, -1);
    uart_driver_install(UART_NUM_2, 1024, 1024, 0, NULL, 0);
    ESP_LOGI(TAG, "UART2 ready (TX=GPIO2 RX=GPIO4)");
}

static void send_model_to_stm32(const uint8_t *model, size_t len, uint32_t version)
{
    uint8_t preamble[4] = {0x55, 0x55, 0x55, 0xAA};  /* 3 training + 1 start */
    uart_write_bytes(UART_NUM_2, (const char *)preamble, 4);
    vTaskDelay(pdMS_TO_TICKS(50));

    ota_frame_header_t hdr = {
        .magic   = OTA_MAGIC,
        .version = version,
        .length  = (uint32_t)len,
    };
        
    uint32_t crc = esp_rom_crc32_le(0, model, len);
    uart_write_bytes(UART_NUM_2, (const char *)&hdr,  sizeof(hdr));
    uart_write_bytes(UART_NUM_2, (const char *)model, len);
    uart_write_bytes(UART_NUM_2, (const char *)&crc,  sizeof(crc));
    ESP_LOGI(TAG, "OTA frame sent: v%"PRIu32" len=%u crc=0x%08"PRIx32,
             version, (unsigned)len, crc);
}

#if 1
void app_main(void)
{
    ESP_LOGI(TAG, "=== SecureInferNode ESP32 Gateway ===");
    nvs_flash_init();

    if (!wifi_init_sta()) {
        ESP_LOGE(TAG, "WiFi failed"); return;
    }

    s_model_len = fetch_file(MODEL_URL, s_model_buf, OTA_MAX_MODEL_SIZE);
    s_sig_len   = fetch_file(SIG_URL,   s_sig_buf,   sizeof(s_sig_buf));

    if (s_model_len == 0 || s_sig_len == 0) {
        ESP_LOGE(TAG, "Fetch failed"); return;
    }

    if (gateway_verify(s_model_buf, s_model_len,
                        s_sig_buf,   s_sig_len) != 0) {
        ESP_LOGE(TAG, "GATEWAY VERIFY FAILED — discarded"); return;
    }
    ESP_LOGI(TAG, "GATEWAY VERIFY OK");

    uart_init();

    vTaskDelay(pdMS_TO_TICKS(500));

    /* TEST: send 20 known bytes */
    // uint8_t test_pattern[20];
    // for (int i = 0; i < 20; i++) test_pattern[i] = 0xA0 + i;
    // int bytes_sent = uart_write_bytes(UART_NUM_2, (const char *)test_pattern, 20);
    // ESP_LOGI(TAG, "Test pattern sent: %d bytes", bytes_sent);

    vTaskDelay(pdMS_TO_TICKS(100));  /* let UART settle */
    send_model_to_stm32(s_model_buf, s_model_len, MODEL_VERSION);

    /* TODO Day 22: SPDM attestation */
    ESP_LOGI(TAG, "TODO Day 22: spdm_get_measurements()");
}
#endif

#if 0
/* UART2 RX test */
void app_main(void)
{
    ESP_LOGI(TAG, "=== UART2 RX test ===");

    uart_config_t cfg = {
        .baud_rate  = 115200,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
    };
    uart_param_config(UART_NUM_2, &cfg);
    uart_set_pin(UART_NUM_2, 2, 4, -1, -1);  /* TX=GPIO2 RX=GPIO4 */
    uart_driver_install(UART_NUM_2, 1024, 1024, 0, NULL, 0);
    ESP_LOGI(TAG, "UART2 ready (TX=GPIO2 RX=GPIO4) - listening...");

    uint8_t buf[64];
    int total = 0;
    uint32_t start = xTaskGetTickCount();

    while ((xTaskGetTickCount() - start) < pdMS_TO_TICKS(15000)) {
        int len = uart_read_bytes(UART_NUM_2, buf, sizeof(buf), pdMS_TO_TICKS(100));
        for (int i = 0; i < len; i++) {
            ESP_LOGI(TAG, "RX byte #%d: 0x%02X", total++, buf[i]);
        }
    }
    ESP_LOGI(TAG, "Test done. Total bytes received: %d", total);
}

#endif

#if 0
/* UART2 TX test */
void app_main(void)
{
    ESP_LOGI(TAG, "=== UART2 TX test ===");

    uart_config_t cfg = {
        .baud_rate  = 115200,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
    };
    uart_param_config(UART_NUM_2, &cfg);
    uart_set_pin(UART_NUM_2, 2, 4, -1, -1);  /* TX=GPIO2 RX=GPIO4 */
    uart_driver_install(UART_NUM_2, 1024, 1024, 0, NULL, 0);
    ESP_LOGI(TAG, "UART2 ready (TX=GPIO2 RX=GPIO4) - sending...");

    for (uint8_t i = 0; i < 50; i++) {
        uart_write_bytes(UART_NUM_2, (const char *)&i, 1);
        ESP_LOGI(TAG, "Sent: 0x%02X", i);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    ESP_LOGI(TAG, "UART2 TX test done");
}
#endif
