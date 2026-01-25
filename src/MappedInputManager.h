#pragma once

#include <InputManager.h>

namespace papyrix {
struct Settings;
}

class MappedInputManager {
 public:
  enum class Button { Back, Confirm, Left, Right, Up, Down, Power, PageBack, PageForward };

  struct Labels {
    const char* btn1;
    const char* btn2;
    const char* btn3;
    const char* btn4;
  };

  explicit MappedInputManager(InputManager& inputManager) : inputManager(inputManager), settings_(nullptr) {}

  void setSettings(papyrix::Settings* settings) { settings_ = settings; }

  bool wasPressed(Button button) const;
  bool wasReleased(Button button) const;
  bool isPressed(Button button) const;
  bool wasAnyPressed() const;
  bool wasAnyReleased() const;
  unsigned long getHeldTime() const;
  Labels mapLabels(const char* back, const char* confirm, const char* previous, const char* next) const;

 private:
  InputManager& inputManager;
  papyrix::Settings* settings_;
  decltype(InputManager::BTN_BACK) mapButton(Button button) const;
};
