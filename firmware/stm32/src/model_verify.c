/*
 * src/model_verify.c — ECDSA P-256 model signature verification
 *
 * Uses mbedTLS (enabled via CONFIG_MBEDTLS=y in prj.conf).
 *
 * TODO Day 11:
 *   1. Run on PC: openssl ec -in keys/public.pem -pubin -noout -text 2>&1
 *   2. Copy the 32-byte X and Y values into PUBLIC_KEY_X / PUBLIC_KEY_Y
 */
#include "model_verify.h"
#include <zephyr/logging/log.h>
#include <mbedtls/ecdsa.h>
#include <mbedtls/sha256.h>
#include <mbedtls/ecp.h>
#include <mbedtls/bignum.h>
#include <mbedtls/error.h>

LOG_MODULE_REGISTER(model_verify, LOG_LEVEL_INF);

/*
 * Hardcoded public key — embedded in firmware at build time.
 * Changing the key requires rebuilding and reflashing firmware.
 * In production: loaded from a write-protected flash region or OTP.
 *
 * TODO Day 11: replace placeholder zeros with real key bytes.
 */
static const uint8_t PUBLIC_KEY_X[32] = {
    /* 32 bytes of X coordinate from: openssl ec -in keys/public.pem ... */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const uint8_t PUBLIC_KEY_Y[32] = {
    /* 32 bytes of Y coordinate */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

int model_verify(const uint8_t *model, size_t model_len,
                 const uint8_t *sig,   size_t sig_len)
{
    int ret;
    uint8_t hash[32];
    mbedtls_ecdsa_context ctx;
    char errbuf[128];

    mbedtls_ecdsa_init(&ctx);

    /* 1. SHA-256 over the entire model binary */
    ret = mbedtls_sha256(model, model_len, hash, 0 /* is224=0 → SHA-256 */);
    if (ret != 0) {
        mbedtls_strerror(ret, errbuf, sizeof(errbuf));
        LOG_ERR("SHA-256 failed: %s", errbuf);
        goto cleanup;
    }

    /* 2. Load P-256 curve */
    ret = mbedtls_ecp_group_load(&ctx.MBEDTLS_PRIVATE(grp),
                                  MBEDTLS_ECP_DP_SECP256R1);
    if (ret != 0) goto cleanup;

    /* 3. Load public key coordinates */
    ret = mbedtls_mpi_read_binary(
        &ctx.MBEDTLS_PRIVATE(Q).MBEDTLS_PRIVATE(X),
        PUBLIC_KEY_X, sizeof(PUBLIC_KEY_X));
    if (ret != 0) goto cleanup;

    ret = mbedtls_mpi_read_binary(
        &ctx.MBEDTLS_PRIVATE(Q).MBEDTLS_PRIVATE(Y),
        PUBLIC_KEY_Y, sizeof(PUBLIC_KEY_Y));
    if (ret != 0) goto cleanup;

    /* Z=1 means affine coordinates (standard for uncompressed public key) */
    ret = mbedtls_mpi_lset(
        &ctx.MBEDTLS_PRIVATE(Q).MBEDTLS_PRIVATE(Z), 1);
    if (ret != 0) goto cleanup;

    /* 4. Verify DER-encoded ECDSA signature */
    ret = mbedtls_ecdsa_read_signature(&ctx, hash, sizeof(hash),
                                        sig, sig_len);
    if (ret == 0) {
        LOG_INF("Signature VALID");
    } else {
        mbedtls_strerror(ret, errbuf, sizeof(errbuf));
        LOG_ERR("Signature INVALID: %s", errbuf);
    }

cleanup:
    mbedtls_ecdsa_free(&ctx);
    return ret;
}
