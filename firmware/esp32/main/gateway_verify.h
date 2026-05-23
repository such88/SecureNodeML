/*
 * firmware/esp32/main/gateway_verify.h
 *
 * ECDSA P-256 verification on ESP32.
 * Same public key and algorithm as STM32 model_verify.c.
 * Implementation in gateway_verify.c
 */
#pragma once
#include <stdint.h>
#include <stddef.h>

/**
 * @brief  Verify ECDSA P-256 signature over model bytes.
 *
 * If this returns non-zero: discard model, do NOT send to STM32.
 * STM32 will do its own independent verify — but gateway is first gate.
 *
 * @return 0 if valid, negative if invalid
 */
int gateway_verify(const uint8_t *model, size_t model_len,
                   const uint8_t *sig,   size_t sig_len);
