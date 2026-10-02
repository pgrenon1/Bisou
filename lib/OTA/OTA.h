#pragma once

// Provides wireless firmware updates through the ESP32's own Wi-Fi network.
//
// begin() creates the access point configured in OTAConfig.h. A phone
// or computer connected to that network can reach the ESP32 at 192.168.4.1,
// which is the default address assigned to an ESP32 access point. It also
// starts ArduinoOTA, which accepts the incoming firmware upload.
//
// ssid and password are in OtaProgrammerConfig.h
//
// update() must be called regularly from loop(). ArduinoOTA uses it to process
// network discovery and upload data while the rest of the firmware continues
// running.
class OTA
{
 public:
  void begin();
  void update();

 private:
  bool started_ = false;
};
