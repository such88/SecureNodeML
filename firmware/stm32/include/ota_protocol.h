/*
 * include/ota_protocol.h — OTA UART frame definition
 *
 * SHARED between STM32 (receiver) and ESP32 (sender).
 * If you change this struct, update BOTH boards.
 *
 * Wire format:
 *   [magic:4B][version:4B][length:4B][payload:length B][crc32:4B]
 */
#pragma once
#include <stdint.h>

#define OTA_MAGIC           0xDEADC0DEU
#define OTA_MAX_MODEL_SIZE  (128U * 1024U)   /* sector 7 = 128KB */

#pragma pack(push, 1)
typedef struct {
    uint32_t magic;     /**< Must equal OTA_MAGIC                    */
    uint32_t version;   /**< Model version — anti-rollback check     */
    uint32_t length;    /**< Payload length in bytes                 */
    /* uint8_t  payload[length];  follows immediately in wire stream */
    /* uint32_t crc32;            follows payload in wire stream     */
} ota_frame_header_t;
#pragma pack(pop)
