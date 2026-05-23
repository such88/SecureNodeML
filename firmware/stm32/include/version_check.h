/*
 * include/version_check.h — Anti-rollback model version API
 *
 * Implementation is in src/version_check.c
 */
#pragma once
#include <stdint.h>

/**
 * @brief  Check incoming version and update stored version if accepted.
 *
 * Rejects if incoming <= stored (anti-rollback).
 * Persists accepted version to NVS (flash-backed, survives power cycle).
 *
 * @param incoming_version  Version number from OTA frame header
 * @return  0 if accepted, -EACCES if rollback detected
 */
int version_check_and_update(uint32_t incoming_version);

/** @return  Current stored version (0 if never set) */
uint32_t version_get_current(void);
