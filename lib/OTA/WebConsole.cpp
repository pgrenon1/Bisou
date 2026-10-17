#include "WebConsole.h"

namespace
{
constexpr size_t MaximumPendingOutputBytes = 8192;

const char ConsolePage[] PROGMEM = R"HTML(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Bisou serial console</title>
  <style>
    body { background: #15171a; color: #e8eaed; font-family: monospace; margin: 0; }
    header { align-items: center; background: #252a30; display: flex; gap: 1rem; padding: .75rem 1rem; }
    button { cursor: pointer; }
    pre { box-sizing: border-box; margin: 0; min-height: calc(100vh - 3.2rem); padding: 1rem; white-space: pre-wrap; word-break: break-word; }
  </style>
</head>
<body>
  <header><strong>Bisou serial console</strong><span id="status">Connecting...</span><button id="clear">Clear</button></header>
  <pre id="output"></pre>
  <script>
    const output = document.getElementById('output');
    const status = document.getElementById('status');
    document.getElementById('clear').onclick = () => output.textContent = '';
    async function readLogs() {
      try {
        const response = await fetch('/logs', { cache: 'no-store' });
        if (!response.ok) throw new Error(response.status);
        const logs = await response.text();
        if (logs) {
          output.textContent += logs;
          window.scrollTo(0, document.body.scrollHeight);
        }
        status.textContent = 'Live';
      } catch (error) {
        status.textContent = 'Reconnecting...';
      }
    }
    setInterval(readLogs, 500);
    readLogs();
  </script>
</body>
</html>
)HTML";
}

WebConsole Console;

// Register the browser UI only after the ESP32 access point is available.
void WebConsole::begin()
{
  if (started_)
    return;

  server_.on("/", HTTP_GET, [this]() {
    // Send a normal response so browsers receive the complete console document.
    server_.sendHeader("Cache-Control", "no-store");
    server_.send(200, "text/html; charset=utf-8", ConsolePage);
  });
  server_.on("/logs", HTTP_GET, [this]() {
    String output = pendingOutput_;
    pendingOutput_ = "";
    server_.send(200, "text/plain; charset=utf-8", output);
  });
  server_.begin();
  started_ = true;
}

void WebConsole::update()
{
  if (started_)
    server_.handleClient();
}

size_t WebConsole::write(uint8_t character)
{
  return write(&character, 1);
}

size_t WebConsole::write(const uint8_t* buffer, size_t size)
{
  const size_t written = Serial.write(buffer, size);
  append(buffer, size);
  return written;
}

// Retain a bounded log so a quiet browser does not consume all ESP32 memory.
void WebConsole::append(const uint8_t* buffer, size_t size)
{
  if (size >= MaximumPendingOutputBytes)
  {
    pendingOutput_ = "";
    pendingOutput_.reserve(MaximumPendingOutputBytes);
    for (size_t index = size - MaximumPendingOutputBytes; index < size; ++index)
      pendingOutput_ += static_cast<char>(buffer[index]);
    return;
  }

  if (pendingOutput_.length() + size > MaximumPendingOutputBytes)
  {
    const size_t excess = pendingOutput_.length() + size - MaximumPendingOutputBytes;
    pendingOutput_.remove(0, excess);
  }

  pendingOutput_.concat(reinterpret_cast<const char*>(buffer), size);
}
