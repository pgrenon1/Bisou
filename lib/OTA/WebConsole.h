#pragma once

#include <Arduino.h>
#include <WebServer.h>

// Mirrors diagnostic output to USB serial and exposes it through the OTA access point.
class WebConsole : public Print
{
 public:
  void begin();
  void update();

  size_t write(uint8_t character) override;
  size_t write(const uint8_t* buffer, size_t size) override;

 private:
  void append(const uint8_t* buffer, size_t size);

  WebServer server_{80};
  String pendingOutput_;
  bool started_ = false;
};

extern WebConsole Console;
