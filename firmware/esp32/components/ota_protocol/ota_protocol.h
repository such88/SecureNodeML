/*
 * components/ota_protocol/ota_protocol.h
 *
 * Shared OTA UART frame definition.
 * Identical to firmware/stm32/include/ota_protocol.h
 * Keep these two files in sync.
 */
#pragma once
#include <stdint.h>

#define OTA_MAGIC           0xDEADC0DEU
#define OTA_MAX_MODEL_SIZE  (128U * 1024U)

#pragma pack(push, 1)
typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t length;
} ota_frame_header_t;
#pragma pack(pop)
