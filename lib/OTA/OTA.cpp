#include "OTA.h"

#include <Arduino.h>
#include <ArduinoOTA.h>
#include <WiFi.h>

#include "OTAConfig.h"
#include "WebConsole.h"

// The factory-programmed eFuse MAC is unique and does not change between boots.
String OTA::GetSSID() const
{
  const uint64_t deviceMac = ESP.getEfuseMac() & 0xFFFFFFFFFFFFULL;
  char hardwareIdentifier[13];
  snprintf(hardwareIdentifier, sizeof(hardwareIdentifier), "%012llX",
           static_cast<unsigned long long>(deviceMac));

  return String(OTAConfig::AccessPointSsid) + "-" + hardwareIdentifier;
}

// Start a private Wi-Fi network to upload code via OTA
void OTA::begin()
{
  // Keep the station interface available because ESP-NOW uses it alongside the OTA AP.
  WiFi.mode(WIFI_AP_STA);
  delay(100);

  String ssid = GetSSID();
  // Stop here when the AP cannot start; ArduinoOTA cannot accept uploads without it.
  if (!WiFi.softAP(ssid, OTAConfig::AccessPointPassword, OTAConfig::WifiChannel))
  {
    Serial.println("OTA access point failed to start");
    return;
  }

  ArduinoOTA.setHostname(OTAConfig::Hostname);
  ArduinoOTA.begin();
  Console.begin();
  started_ = true;
  Console.print("OTA access point: ");
  Console.println(ssid);
  Console.print("OTA upload address: ");
  Console.println(WiFi.softAPIP());
  Console.println("Web serial console: http://192.168.4.1/");
}

// Handle pending discovery and firmware-upload requests on each main-loop pass.
void OTA::update()
{
  if (started_)
  {
    ArduinoOTA.handle();
    Console.update();
  }
}
