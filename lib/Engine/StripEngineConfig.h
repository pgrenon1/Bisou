#pragma once

#include <FastLED.h>

// Hardware configurations
namespace StripEngineConfig
{
    template <uint8_t DataPin, EOrder ColorOrder>
    using LedChipset = WS2811<DataPin, ColorOrder>;
    constexpr uint8_t LedPin = 19;
    constexpr uint16_t LedCount = 90;
    constexpr EOrder ColorOrder = RGB;
    // ADC1 input-only pin on common ESP32 DevKit boards; leave unconnected for
    // a noise-based seed. Change this if your board uses GPIO 34 for something else.
    constexpr uint8_t RandomSeedPin = 34;
    constexpr uint8_t Brightness = 255;
}
