#pragma once
#include <stdint.h>

constexpr uint32_t AUTO_LOCK_TIME_MS    = 5000UL;
constexpr uint32_t OPEN_TIMEOUT_MS       = 30000UL;
constexpr uint32_t MAX_OPEN_TIMEOUT_MS   = 300000UL;
constexpr uint32_t OPEN_NIGHT_TIME_MS    = 7200000UL;
constexpr uint32_t LOCK_TIME_MS          = 5000UL;
constexpr uint32_t UNLOCK_TIME_MS        = 5000UL;
constexpr uint32_t RETRY_DELAY_MS        = 1000UL;

constexpr uint32_t DEBOUNCE_DOOR_MS      = 30UL;
constexpr uint32_t DEBOUNCE_BOLT_MS      = 20UL;
constexpr uint32_t DEBOUNCE_RFID_MS      = 10UL;
constexpr uint32_t DEBOUNCE_EXIT_MS      = 30UL;
constexpr uint32_t DEBOUNCE_MODE_MS      = 1000UL;
