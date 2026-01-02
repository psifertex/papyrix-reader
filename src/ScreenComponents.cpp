#include "ScreenComponents.h"

#include <GfxRenderer.h>

#include "Battery.h"
#include "ThemeManager.h"

void ScreenComponents::drawBattery(const GfxRenderer& renderer, int x, int y) {
  const uint16_t millivolts = battery.readMillivolts();

  // Battery icon dimensions
  constexpr int batteryWidth = 15;
  constexpr int batteryHeight = 10;
  constexpr int spacing = 5;

  // Draw battery icon
  // Top line
  renderer.drawLine(x, y, x + batteryWidth - 4, y, THEME.primaryTextBlack);
  // Bottom line
  renderer.drawLine(x, y + batteryHeight - 1, x + batteryWidth - 4, y + batteryHeight - 1, THEME.primaryTextBlack);
  // Left line
  renderer.drawLine(x, y, x, y + batteryHeight - 1, THEME.primaryTextBlack);
  // Right line
  renderer.drawLine(x + batteryWidth - 4, y, x + batteryWidth - 4, y + batteryHeight - 1, THEME.primaryTextBlack);
  // Battery nub (right side)
  renderer.drawLine(x + batteryWidth - 3, y + 2, x + batteryWidth - 1, y + 2, THEME.primaryTextBlack);
  renderer.drawLine(x + batteryWidth - 3, y + batteryHeight - 3, x + batteryWidth - 1, y + batteryHeight - 3,
                    THEME.primaryTextBlack);
  renderer.drawLine(x + batteryWidth - 1, y + 2, x + batteryWidth - 1, y + batteryHeight - 3, THEME.primaryTextBlack);

  // Check for valid reading and calculate percentage
  char percentageText[8];
  uint16_t percentage = 0;
  if (millivolts < 100) {
    // Invalid reading - show error indicator
    Serial.printf("[BAT] Invalid reading: millivolts=%u, showing --%%\n", millivolts);
    snprintf(percentageText, sizeof(percentageText), "--%%");
  } else {
    percentage = BatteryMonitor::percentageFromMillivolts(millivolts);
    Serial.printf("[BAT] millivolts=%u, percentage=%u%%\n", millivolts, percentage);
    snprintf(percentageText, sizeof(percentageText), "%u%%", percentage);
  }

  // Fill level (proportional to percentage)
  int filledWidth = percentage * (batteryWidth - 5) / 100 + 1;
  if (filledWidth > batteryWidth - 5) {
    filledWidth = batteryWidth - 5;
  }
  renderer.fillRect(x + 1, y + 1, filledWidth, batteryHeight - 2, THEME.primaryTextBlack);

  // Draw percentage text to the right of the icon
  renderer.drawText(THEME.smallFontId, x + batteryWidth + spacing, y, percentageText, THEME.primaryTextBlack);
}
