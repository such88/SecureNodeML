/*
 * include/model_verify.h — ECDSA P-256 model verification API
 *
 * Include this in any file that needs to call model_verify().
 * Implementation is in src/model_verify.c
 */
#pragma once
#include <stdint.h>
#include <stddef.h>

/**
 * @brief  Verify ECDSA P-256 signature over a model binary.
 *
 * Call at boot BEFORE creating the inference thread.
 * On failure: caller MUST call k_panic() — do not allow inference to start.
 *
 * @param model     Pointer to model binary in flash
 * @param model_len Size of model binary in bytes
 * @param sig       DER-encoded ECDSA P-256 signature
 * @param sig_len   Length of signature (max 72 bytes for P-256)
 * @return          0 on success, negative on failure
 */
int model_verify(const uint8_t *model, size_t model_len,
                 const uint8_t *sig,   size_t sig_len);
