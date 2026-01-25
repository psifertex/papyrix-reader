#pragma once

#include <cstddef>
#include <cstdint>

#include "../core/Result.h"

namespace papyrix {
namespace drivers {

// Network driver - ONLY used for book sync (Calibre, OPDS, HTTP transfer)
// WiFi fragments heap - device must restart after any network use
class Network {
 public:
  Result<void> init();
  void shutdown();

  bool isInitialized() const { return initialized_; }
  bool isConnected() const { return connected_; }

  Result<void> connect(const char* ssid, const char* password);
  void disconnect();

  // WiFi fragments heap - always true after any WiFi use
  bool needsRestart() const { return initialized_; }

  // Get signal strength (RSSI)
  int8_t signalStrength() const;

  // Get IP address as string
  void getIpAddress(char* buffer, size_t bufferSize) const;

 private:
  bool initialized_ = false;
  bool connected_ = false;
};

}  // namespace drivers
}  // namespace papyrix
