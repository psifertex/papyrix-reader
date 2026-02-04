#pragma once

#include <BackgroundTask.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <atomic>

#include "../states/State.h"
#include "Types.h"

namespace papyrix {

struct Core;

class StateMachine {
 public:
  void init(Core& core, StateId initialState = StateId::Startup);
  void update(Core& core);

  StateId currentStateId() const { return currentId_; }
  bool isInState(StateId id) const { return currentId_ == id; }

  // Register state instances (called during setup)
  void registerState(State* state);

  // Render control - call to signal that a render is needed
  void requestRender() { renderRequested_.store(true, std::memory_order_release); }

  // Mutex access for states that need direct synchronized renderer access
  bool takeRenderLock(TickType_t timeout = portMAX_DELAY);
  void releaseRenderLock();

 private:
  State* getState(StateId id);
  void transition(StateId next, Core& core, bool immediate);

  // Display task management
  void startDisplayTask(Core& core);
  void stopDisplayTask();
  void displayTaskLoop();

  State* current_ = nullptr;
  StateId currentId_ = StateId::Startup;

  // State registry - pointers to pre-allocated state instances
  static constexpr size_t MAX_STATES = 10;
  State* states_[MAX_STATES] = {};
  size_t stateCount_ = 0;

  // Display task infrastructure
  BackgroundTask displayTask_;
  SemaphoreHandle_t renderMutex_ = nullptr;
  std::atomic<bool> renderRequested_{false};
  Core* corePtr_ = nullptr;  // Snapshot for background task access

  static constexpr uint32_t kDisplayTaskStack = 8192;
  static constexpr int kDisplayTaskPriority = 1;
  static constexpr TickType_t kRenderPollInterval = pdMS_TO_TICKS(10);
  static constexpr uint32_t kTransitionTimeoutMs = 1000;
};

}  // namespace papyrix
