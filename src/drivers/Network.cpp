#include "Network.h"

#include <Arduino.h>
#include <WiFi.h>

namespace papyrix {
namespace drivers {

Result<void> Network::init() {
  if (initialized_) {
    return Ok();
  }

  WiFi.mode(WIFI_STA);
  initialized_ = true;
  connected_ = false;

  Serial.println("[NET] WiFi initialized (STA mode)");
  return Ok();
}

void Network::shutdown() {
  if (connected_) {
    disconnect();
  }

  if (initialized_) {
    WiFi.mode(WIFI_OFF);
    initialized_ = false;
    Serial.println("[NET] WiFi shut down");
  }
}

Result<void> Network::connect(const char* ssid, const char* password) {
  if (!initialized_) {
    TRY(init());
  }

  Serial.printf("[NET] Connecting to %s...\n", ssid);

  WiFi.begin(ssid, password);

  // Wait for connection with timeout
  constexpr uint32_t TIMEOUT_MS = 15000;
  uint32_t startMs = millis();

  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - startMs > TIMEOUT_MS) {
      Serial.println("[NET] Connection timeout");
      return ErrVoid(Error::Timeout);
    }
    delay(100);
  }

  connected_ = true;
  Serial.printf("[NET] Connected, IP: %s\n", WiFi.localIP().toString().c_str());
  return Ok();
}

void Network::disconnect() {
  if (connected_) {
    WiFi.disconnect();
    connected_ = false;
    Serial.println("[NET] Disconnected");
  }
}

int8_t Network::signalStrength() const {
  if (!connected_) {
    return 0;
  }
  return WiFi.RSSI();
}

void Network::getIpAddress(char* buffer, size_t bufferSize) const {
  if (!connected_ || bufferSize == 0) {
    if (bufferSize > 0) buffer[0] = '\0';
    return;
  }

  String ip = WiFi.localIP().toString();
  strncpy(buffer, ip.c_str(), bufferSize - 1);
  buffer[bufferSize - 1] = '\0';
}

}  // namespace drivers
}  // namespace papyrix
