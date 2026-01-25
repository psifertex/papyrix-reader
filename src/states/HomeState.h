#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <atomic>
#include <cstdint>
#include <string>

#include "../ui/views/HomeView.h"
#include "State.h"

class GfxRenderer;

namespace papyrix {

class HomeState : public State {
 public:
  explicit HomeState(GfxRenderer& renderer);
  ~HomeState() override;

  void enter(Core& core) override;
  void exit(Core& core) override;
  StateTransition update(Core& core) override;
  void render(Core& core) override;
  StateId id() const override { return StateId::Home; }

 private:
  GfxRenderer& renderer_;
  ui::HomeView view_;

  // Cover image state
  std::string coverBmpPath_;
  bool hasCoverImage_ = false;
  bool coverLoadFailed_ = false;

  // Buffer caching for fast navigation
  uint8_t* coverBuffer_ = nullptr;
  bool coverBufferStored_ = false;
  bool coverRendered_ = false;

  // Async cover generation
  TaskHandle_t coverGenTaskHandle_ = nullptr;
  std::atomic<bool> coverGenComplete_{false};
  std::string pendingBookPath_;
  std::string pendingCacheDir_;
  std::string generatedCoverPath_;  // Written by task, read after coverGenComplete_

  void loadLastBook(Core& core);
  void updateBattery();
  void renderCoverToCard();
  void startCoverGenTask(const char* bookPath, const char* cacheDir);
  void stopCoverGenTask();
  static void coverGenTrampoline(void* arg);
  void coverGenTask();

  // Buffer caching methods
  bool storeCoverBuffer();
  bool restoreCoverBuffer();
  void freeCoverBuffer();
};

}  // namespace papyrix
