#include "OTA.h"

#include <Arduino.h>
#include <ArduinoOTA.h>
#include <WiFi.h>

#include "OTAConfig.h"

// Start a private Wi-Fi network to upload code via OTA
void OTA::begin()
{
  // Keep the station interface available because ESP-NOW uses it alongside the OTA AP.
  WiFi.mode(WIFI_AP_STA);
  delay(100);

  // Stop here when the AP cannot start; ArduinoOTA cannot accept uploads without it.
  if (!WiFi.softAP(OTAConfig::AccessPointSsid, OTAConfig::AccessPointPassword))
  {
    Serial.println("OTA access point failed to start");
    return;
  }

  ArduinoOTA.setHostname(OTAConfig::Hostname);
  ArduinoOTA.begin();
  started_ = true;
  Serial.print("OTA access point: ");
  Serial.println(OTAConfig::AccessPointSsid);
  Serial.print("OTA upload address: ");
  Serial.println(WiFi.softAPIP());
}

// Handle pending discovery and firmware-upload requests on each main-loop pass.
void OTA::update()
{
  if (started_)
    ArduinoOTA.handle();
}
