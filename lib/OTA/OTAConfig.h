#pragma once

#include <stdint.h>

namespace OTAConfig
{
    constexpr char AccessPointSsid[] = "Bisou-OTA";
    constexpr char AccessPointPassword[] = "bisou_xoxo";
    constexpr char Hostname[] = "bisou";
    // ESP-NOW and the OTA access point must share one fixed Wi-Fi channel.
    constexpr uint8_t WifiChannel = 1;
}
