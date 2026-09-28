#pragma once

#include <Arduino.h>

// Inputs
constexpr uint8_t PIN_LOCK_STATE      = 2;  // Active-low: PR12-4DN lock-state sensor
constexpr uint8_t PIN_EXIT_BUTTON     = 3;  // Active-low: internal exit button
constexpr uint8_t PIN_DOOR_CLOSED     = 4;  // Active-low: door-closed reed switch
constexpr uint8_t PIN_UNUSED_D5       = 5;
constexpr uint8_t PIN_CARD_READER_J5  = 7;  // Active-low release signal from BlueBoard

// 3-position maintained key switch, centre off.
// Each selected contact is expected to pull its input LOW.
// Neither selected      -> Disabled
// Standard selected     -> Standard
// Open Night selected   -> Open Night
// Both selected         -> Invalid / wiring fault
constexpr uint8_t PIN_MODE_STANDARD    = 8;
constexpr uint8_t PIN_MODE_OPEN_NIGHT = 9;

// Relay outputs.
// HIGH energises the relay, pulling the corresponding Utopic puck input LOW.
constexpr uint8_t PIN_LOCK_RELAY       = A3; // RL4 -> PUCK T1
constexpr uint8_t PIN_UNLOCK_RELAY     = A2; // RL3 -> PUCK T2

// Existing spare/other relay channels.
constexpr uint8_t PIN_FAULT_RELAY      = A1; // RL2 -> FAULT connector (currently unused)
constexpr uint8_t PIN_BLUEBOARD_BUTTON = A0; // RL1 -> BlueBoard "Button" contacts
