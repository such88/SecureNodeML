/*
 * firmware/esp32/main/gateway_verify.c
 *
 * ECDSA P-256 verification on ESP32 using mbedTLS (built into ESP-IDF).
 * Identical algorithm to STM32 model_verify.c.
 * Same hardcoded public key — must stay in sync with STM32 side.
 *
 * TODO Day 21: fill PUBLIC_KEY_X and PUBLIC_KEY_Y
 * (same values as in firmware/stm32/src/model_verify.c)
 */
#include "gateway_verify.h"
#include "esp_log.h"
#include "mbedtls/ecdsa.h"
#include "mbedtls/sha256.h"
#include "mbedtls/ecp.h"
#include "mbedtls/bignum.h"
#include "mbedtls/error.h"

static const char *TAG = "gw_verify";

/* TODO Day 21: copy from firmware/stm32/src/model_verify.c */
static const uint8_t PUBLIC_KEY_X[32] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
static const uint8_t PUBLIC_KEY_Y[32] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

int gateway_verify(const uint8_t *model, size_t model_len,
                   const uint8_t *sig,   size_t sig_len)
{
    int ret;
    uint8_t hash[32];
    char errbuf[128];
    mbedtls_ecdsa_context ctx;

    mbedtls_ecdsa_init(&ctx);

    ret = mbedtls_sha256(model, model_len, hash, 0);
    if (ret != 0) {
        mbedtls_strerror(ret, errbuf, sizeof(errbuf));
        ESP_LOGE(TAG, "SHA-256 failed: %s", errbuf);
        goto cleanup;
    }

    ret = mbedtls_ecp_group_load(&ctx.MBEDTLS_PRIVATE(grp),
                                  MBEDTLS_ECP_DP_SECP256R1);
    if (ret != 0) goto cleanup;

    ret = mbedtls_mpi_read_binary(
        &ctx.MBEDTLS_PRIVATE(Q).MBEDTLS_PRIVATE(X), PUBLIC_KEY_X, 32);
    if (ret != 0) goto cleanup;

    ret = mbedtls_mpi_read_binary(
        &ctx.MBEDTLS_PRIVATE(Q).MBEDTLS_PRIVATE(Y), PUBLIC_KEY_Y, 32);
    if (ret != 0) goto cleanup;

    mbedtls_mpi_lset(&ctx.MBEDTLS_PRIVATE(Q).MBEDTLS_PRIVATE(Z), 1);

    ret = mbedtls_ecdsa_read_signature(&ctx, hash, sizeof(hash),
                                        sig, sig_len);
    if (ret == 0) {
        ESP_LOGI(TAG, "GATEWAY VERIFY OK — forwarding to STM32");
    } else {
        mbedtls_strerror(ret, errbuf, sizeof(errbuf));
        ESP_LOGE(TAG, "GATEWAY VERIFY FAILED: %s — model discarded", errbuf);
    }

cleanup:
    mbedtls_ecdsa_free(&ctx);
    return ret;
}
