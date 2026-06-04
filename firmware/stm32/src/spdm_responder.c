/*
 * src/spdm_responder.c — SPDM 1.2 message handler (DSP0274)
 *
 * Transport: USART1 at 115200 baud (PA9 TX / PA10 RX)
 * Implements:
 *   GET_VERSION      → returns supported SPDM versions (1.0, 1.2)
 *   GET_CAPABILITIES → returns responder capabilities
 *   GET_MEASUREMENTS → returns SHA-384 of model binary (measurement index 1)
 *
 * Test with: python scripts/signing/spdm_host.py /dev/ttyACM0
 * Spec: DMTF DSP0274 v1.2 — free at dmtf.org/dsp/DSP0274
 *
 * TODO Days 15–16: complete each handler, add USART1 receive loop
 */
#include "spdm_responder.h"
#include <zephyr/logging/log.h>
#include <mbedtls/sha512.h>
#include <string.h>

LOG_MODULE_REGISTER(spdm, LOG_LEVEL_DBG);

/* Model location in flash — same address as used in main.c */
#define MODEL_BASE_ADDR  0x08060000U
//#define MODEL_LEN        (120U * 1024U) /* TODO: read from model header */
#define MODEL_LEN        4104U

/* Supported SPDM versions: 1.0 and 1.2 */
static const uint8_t VERSION_ENTRIES[] = { 0x10, 0x00, 0x12, 0x00 };

/* ── Handler: GET_VERSION (DSP0274 §10.4) ─────────────────────────────── */
static int handle_get_version(uint8_t *out, size_t out_max)
{
    /*
     * VERSION response wire format:
     * [SPDMVersion=0x10][Code=0x04][P1=0x00][P2=0x00]
     * [Reserved 4B][VersionNumberEntryCount:2B][Entries...]
     */
    const size_t len = 12U;
    if (out_max < len) return -1;

    memset(out, 0, len);
    out[0] = 0x10;                  /* SPDM version used for negotiation */
    out[1] = SPDM_VERSION;          /* response code */
    out[8] = 0x02; out[9] = 0x00;   /* 2 version entries */
    memcpy(&out[10], VERSION_ENTRIES, 4);

    LOG_DBG("Responding: VERSION (supports 1.0, 1.2)");
    return (int)len;
}

/* ── Handler: GET_MEASUREMENTS (DSP0274 §10.11) ──────────────────────── */
static int handle_get_measurements(uint8_t *out, size_t out_max)
{
    /*
     * Measurement block:
     *   Index=1 (model binary digest)
     *   Type=0x01 (immutable ROM/flash digest)
     *   SHA-384 (48 bytes) of model at MODEL_BASE_ADDR
     *
     * TODO Day 16: add full response header + ECDSA signature over response
     * For now: minimal response for testing with spdm_host.py
     */
    const size_t hdr_len  = 4U;
    const size_t hash_len = 48U;    /* SHA-384 = 48 bytes */
    const size_t total    = hdr_len + hash_len;

    if (out_max < total) return -1;

    uint8_t digest[48];
    const uint8_t *model = (const uint8_t *)MODEL_BASE_ADDR;

    /* SHA-384 = mbedtls_sha512(..., is384=1) */
    if (mbedtls_sha512(model, MODEL_LEN, digest, 1) != 0) {
        LOG_ERR("SHA-384 of model failed");
        return -1;
    }

    out[0] = 0x12;              /* SPDM version 1.2 */
    out[1] = SPDM_MEASUREMENTS; /* response code */
    out[2] = 0x00;
    out[3] = 0x01;              /* NumberOfBlocks = 1 */
    memcpy(&out[4], digest, hash_len);

    LOG_INF("MEASUREMENTS: SHA-384 computed over %u bytes at 0x%08X",
            MODEL_LEN, MODEL_BASE_ADDR);
    return (int)total;
}

/* ── Main dispatcher ──────────────────────────────────────────────────── */
int spdm_process_request(const uint8_t *req, size_t req_len,
                          uint8_t *out_buf, size_t out_max)
{
    if (!req || req_len < 4 || !out_buf || out_max == 0) {
        return -1;
    }

    uint8_t code = req[1];
    LOG_DBG("SPDM request: code=0x%02X len=%u", code, (unsigned)req_len);

    switch (code) {
    case SPDM_GET_VERSION:
        return handle_get_version(out_buf, out_max);
    case SPDM_GET_MEASUREMENTS:
        return handle_get_measurements(out_buf, out_max);
    default:
        LOG_WRN("Unhandled SPDM code: 0x%02X", code);
        return -1;
    }
}
