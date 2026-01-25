#include "NetworkViews.h"

#include <cstdio>

namespace ui {

// Static definitions
constexpr const char* const NetworkModeView::ITEMS[];

void render(const GfxRenderer& r, const Theme& t, const NetworkModeView& v) {
  r.clearScreen(t.backgroundColor);

  title(r, t, t.screenMarginTop, "Network Mode");

  const int startY = 100;
  for (int i = 0; i < NetworkModeView::ITEM_COUNT; i++) {
    const int y = startY + i * (t.itemHeight + 20);
    menuItem(r, t, y, NetworkModeView::ITEMS[i], i == v.selected);
  }

  // Description below options
  const int descY = startY + 2 * (t.itemHeight + 20) + 40;
  if (v.selected == 0) {
    centeredText(r, t, descY, "Connect to existing WiFi");
    centeredText(r, t, descY + 25, "for Calibre or OPDS");
  } else {
    centeredText(r, t, descY, "Create WiFi hotspot");
    centeredText(r, t, descY + 25, "for file transfer via browser");
  }

  buttonBar(r, t, "Back", "Select", "", "");

  r.displayBuffer();
}

void render(const GfxRenderer& r, const Theme& t, const WifiListView& v) {
  r.clearScreen(t.backgroundColor);

  title(r, t, t.screenMarginTop, "Select Network");

  if (v.scanning) {
    const int centerY = r.getScreenHeight() / 2;
    centeredText(r, t, centerY, "Scanning...");
  } else if (v.networkCount == 0) {
    const int centerY = r.getScreenHeight() / 2;
    centeredText(r, t, centerY, "No networks found");
    centeredText(r, t, centerY + 30, "Press Confirm to scan again");
  } else {
    const int listStartY = 60;
    const int pageStart = v.getPageStart();
    const int pageEnd = v.getPageEnd();

    for (int i = pageStart; i < pageEnd; i++) {
      const int y = listStartY + (i - pageStart) * (t.itemHeight + t.itemSpacing);
      wifiEntry(r, t, y, v.networks[i].ssid, v.networks[i].signal, v.networks[i].secured, i == v.selected);
    }
  }

  buttonBar(r, t, "Back", "Connect", "Scan", "");

  r.displayBuffer();
}

void render(const GfxRenderer& r, const Theme& t, const WifiConnectingView& v) {
  r.clearScreen(t.backgroundColor);

  title(r, t, t.screenMarginTop, "Connecting");

  const int centerY = r.getScreenHeight() / 2 - 60;

  // SSID
  centeredText(r, t, centerY, v.ssid);

  // Status message
  centeredText(r, t, centerY + 40, v.statusMsg);

  // IP address if connected
  if (v.status == WifiConnectingView::Status::Connected) {
    char ipLine[32];
    snprintf(ipLine, sizeof(ipLine), "IP: %s", v.ipAddress);
    centeredText(r, t, centerY + 80, ipLine);
  }

  // Show appropriate button hints
  if (v.status == WifiConnectingView::Status::Failed) {
    buttonBar(r, t, "Back", "Retry", "", "");
  } else if (v.status == WifiConnectingView::Status::Connected) {
    buttonBar(r, t, "Back", "Done", "", "");
  } else {
    buttonBar(r, t, "Cancel", "", "", "");
  }

  r.displayBuffer();
}

void render(const GfxRenderer& r, const Theme& t, const CalibreView& v) {
  r.clearScreen(t.backgroundColor);

  title(r, t, t.screenMarginTop, "Calibre");

  const int centerY = r.getScreenHeight() / 2 - 60;

  // Status message
  centeredText(r, t, centerY, v.statusMsg);

  // Progress bar if receiving
  if (v.status == CalibreView::Status::Receiving && v.total > 0) {
    progress(r, t, centerY + 50, v.received, v.total);

    // Size info
    char sizeStr[32];
    snprintf(sizeStr, sizeof(sizeStr), "%d / %d KB", v.received / 1024, v.total / 1024);
    centeredText(r, t, centerY + 100, sizeStr);
  }

  // Button hints based on status
  if (v.status == CalibreView::Status::Complete || v.status == CalibreView::Status::Error) {
    buttonBar(r, t, "Back", "", "", "");
  } else {
    buttonBar(r, t, "Cancel", "", "", "");
  }

  r.displayBuffer();
}

void render(const GfxRenderer& r, const Theme& t, const WebServerView& v) {
  r.clearScreen(t.backgroundColor);

  title(r, t, t.screenMarginTop, "Web Server");

  const int lineHeight = r.getLineHeight(t.uiFontId) + 10;
  const int startY = 80;

  if (v.serverRunning) {
    int currentY = startY;
    // SSID
    twoColumnRow(r, t, currentY, "Network:", v.ssid);
    currentY += lineHeight;

    // IP Address (web access URL)
    char urlStr[32];
    snprintf(urlStr, sizeof(urlStr), "http://%s", v.ipAddress);
    twoColumnRow(r, t, currentY, "URL:", urlStr);
    currentY += lineHeight;

    // Client count
    char clientStr[8];
    snprintf(clientStr, sizeof(clientStr), "%d", v.clientCount);
    twoColumnRow(r, t, currentY, "Clients:", clientStr);
    currentY += lineHeight;

    // Instructions
    currentY += 30;
    centeredText(r, t, currentY, "Open URL in browser to");
    centeredText(r, t, currentY + 25, "transfer files");
  } else {
    centeredText(r, t, startY + 100, "Server stopped");
  }

  buttonBar(r, t, "Stop", "", "", "");

  r.displayBuffer();
}

}  // namespace ui
