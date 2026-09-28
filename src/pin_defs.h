#pragma once
#include <Arduino.h>

// Inputs
constexpr uint8_t PIN_LOCK_STATE      = 2;  // -> FLOW -> DIN J12: lock state sensor, green/green pair
constexpr uint8_t PIN_EXIT_BUTTON     = 3;  // -> PFAIL -> DIN J8: door exit button, orange/orange pair
constexpr uint8_t PIN_DOOR_CLOSED     = 4;  // -> AUXI1 -> DIN J10: door closed sensor, green/green pair
constexpr uint8_t PIN_CARD_READER_J5  = 7;  // -> SOUNDER -> DIN J5 / white3: "to Card Reader"

// 3-position key switch on existing AUTO/MAN connector.
// 00 = Disabled, 10 = Standard, 01 = Open Night, 11 = Invalid/fault.
constexpr uint8_t PIN_MODE_STANDARD    = 8;
constexpr uint8_t PIN_MODE_OPEN_NIGHT = 9;

// Relay outputs
constexpr uint8_t PIN_LOCK_RELAY       = A3; // RL4 -> AUX02 -> PUCK T1
constexpr uint8_t PIN_UNLOCK_RELAY     = A2; // RL3 -> AUX01 ->  PUCK T2
constexpr uint8_t PIN_FAULT_RELAY      = A1; // RL2 -> FAULT connector (currently unused)
constexpr uint8_t PIN_BLUEBOARD_BUTTON = A0; // RL1 ->  ALARM -> BlueBoard "Button" contacts

// Known unconnected Nano GPIO on current PCB
constexpr uint8_t PIN_UNUSED_D5 = 5;
constexpr uint8_t PIN_UNUSED_D6 = 6;
// A4 - A7 and B0/Tx + D1/Rx
