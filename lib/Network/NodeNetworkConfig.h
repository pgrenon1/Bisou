#pragma once

#include <Arduino.h>

namespace NodeNetworkConfig
{
  // Wire the node's active-low game input between this pin and ground.
  constexpr uint8_t InputButtonPin = 27;
  constexpr uint16_t InputDebounceTimeMs = 50;
  // Holding the game input during boot starts pairing.
  constexpr uint8_t PairingButtonPin = InputButtonPin;
  // The ESP32 DevKit's onboard status LED is wired to GPIO 2.
  constexpr uint8_t StatusLedPin = 2;
  constexpr uint32_t StatusLedHeartbeatPeriodMs = 1000;
  constexpr uint32_t StatusLedHeartbeatOnDurationMs = 100;
  constexpr uint32_t StatusLedPairingFlashPeriodMs = 50;
  constexpr uint32_t PairingHoldDurationMs = 5000;
  constexpr uint32_t PairingWindowDurationMs = 60000;
  constexpr uint32_t HomeDiscoveryDurationMs = 1500;
  constexpr uint32_t HomeAdvertisementIntervalMs = 500;
  constexpr uint8_t MaxRemoteNodes = 10;
  constexpr uint8_t MaxEventPayloadBytes = 200;
}
