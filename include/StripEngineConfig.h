#pragma once

#include <FastLED.h>

// Hardware configurations
namespace StripEngineConfig
{
    template <uint8_t DataPin, EOrder ColorOrder>
    using LedChipset = WS2811<DataPin, ColorOrder>;
    // GPIO numbers for a common ESP32 DevKit board.
    constexpr uint8_t LedPin = 18;
    constexpr uint16_t LedCount = 90;
    constexpr EOrder ColorOrder = RGB;
    constexpr uint8_t LeftButtonPin = 25;
    constexpr uint8_t RightButtonPin = 26;
    // ADC1 input-only pin on common ESP32 DevKit boards; leave unconnected for
    // a noise-based seed. Change this if your board uses GPIO 34 for something else.
    constexpr uint8_t RandomSeedPin = 34;
    constexpr uint8_t Brightness = 255;
}
