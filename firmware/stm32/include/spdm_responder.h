/*
 * include/spdm_responder.h — SPDM 1.2 responder API
 *
 * SPDM message codes (DSP0274 v1.2 subset)
 * Implementation is in src/spdm_responder.c
 */
#pragma once
#include <stdint.h>
#include <stddef.h>

#define SPDM_GET_VERSION       0x84U  /* requester → responder */
#define SPDM_VERSION           0x04U  /* responder → requester */
#define SPDM_GET_CAPABILITIES  0xE1U
#define SPDM_CAPABILITIES      0x61U
#define SPDM_GET_MEASUREMENTS  0xE0U
#define SPDM_MEASUREMENTS      0x60U

/**
 * @brief  Process one SPDM request and write response.
 *
 * Call from a UART receive ISR or dedicated SPDM listener thread.
 *
 * @param req     Received SPDM request bytes
 * @param req_len Length of request
 * @param out_buf Buffer to write response into
 * @param out_max Size of out_buf
 * @return        Bytes written to out_buf, negative on error
 */
int spdm_process_request(const uint8_t *req, size_t req_len,
                          uint8_t *out_buf, size_t out_max);
