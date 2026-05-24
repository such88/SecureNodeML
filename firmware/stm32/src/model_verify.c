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
    0x77, 0x1f, 0x90, 0x15, 0xeb, 0x30, 0x05, 0x67,
    0xea, 0x97, 0x6b, 0x16, 0xa7, 0xa6, 0xfc, 0x46,
    0x0b, 0xfe, 0xbb, 0x92, 0x0e, 0x18, 0xa5, 0x89,
    0x12, 0x88, 0x41, 0xef, 0x04, 0xab, 0xb3, 0x7c
};

static const uint8_t PUBLIC_KEY_Y[32] = {
    0xe7, 0x73, 0xad, 0xb4, 0x33, 0x4f, 0x82, 0xc7,
    0x96, 0x35, 0x33, 0x49, 0xf0, 0x47, 0x80, 0xd2,
    0xc9, 0x08, 0xd8, 0xc3, 0x92, 0xec, 0x1e, 0xf3,
    0x36, 0x46, 0xa8, 0xc0, 0x56, 0xa6, 0x34, 0x87
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
