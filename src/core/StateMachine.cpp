#include "StateMachine.h"

#include <Arduino.h>
#include <ScopedMutex.h>

#include "Core.h"

namespace papyrix {

void StateMachine::init(Core& core, StateId initialState) {
  // Stop display task if running (e.g., re-init for sleep from any state)
  stopDisplayTask();

  // Create mutex once if not already created
  if (!renderMutex_) {
    renderMutex_ = xSemaphoreCreateMutex();
    // Share mutex with Core so states can synchronize with display task
    core.renderMutex = renderMutex_;
  }

  // Exit current state if one exists (e.g., when triggering sleep from any state)
  if (current_) {
    current_->exit(core);
  }

  currentId_ = initialState;
  current_ = getState(initialState);

  if (current_) {
    Serial.printf("[SM] Initial state: %d\n", static_cast<int>(initialState));
    current_->enter(core);
    // Start display task and trigger initial render
    startDisplayTask(core);
    requestRender();
  } else {
    Serial.printf("[SM] ERROR: No state registered for id %d\n", static_cast<int>(initialState));
  }
}

void StateMachine::update(Core& core) {
  if (!current_) {
    return;
  }

  StateTransition trans = current_->update(core);

  if (trans.next != currentId_) {
    transition(trans.next, core, trans.immediate);
  }

  // Signal display task to check for pending renders
  // States set needsRender_=true during update(); this ensures the display task wakes
  requestRender();
}

void StateMachine::registerState(State* state) {
  if (!state) return;

  if (stateCount_ >= MAX_STATES) {
    Serial.println("[SM] ERROR: Too many states registered");
    return;
  }

  states_[stateCount_++] = state;
  Serial.printf("[SM] Registered state: %d\n", static_cast<int>(state->id()));
}

State* StateMachine::getState(StateId id) {
  for (size_t i = 0; i < stateCount_; ++i) {
    if (states_[i] && states_[i]->id() == id) {
      return states_[i];
    }
  }
  return nullptr;
}

void StateMachine::transition(StateId next, Core& core, bool immediate) {
  State* nextState = getState(next);

  if (!nextState) {
    Serial.printf("[SM] ERROR: No state for id %d\n", static_cast<int>(next));
    return;
  }

  Serial.printf("[SM] Transition: %d -> %d%s\n", static_cast<int>(currentId_), static_cast<int>(next),
                immediate ? " (immediate)" : "");

  // 1. Prevent new renders and wait for current render to complete
  renderRequested_.store(false, std::memory_order_release);
  stopDisplayTask();

  // 2. Perform state transition
  if (current_) {
    current_->exit(core);
  }

  currentId_ = next;
  current_ = nextState;
  current_->enter(core);

  // 3. Resume display task and trigger initial render for new state
  startDisplayTask(core);
  requestRender();
}

void StateMachine::startDisplayTask(Core& core) {
  if (displayTask_.isRunning()) {
    return;
  }

  corePtr_ = &core;
  displayTask_.start("Display", kDisplayTaskStack, [this]() { displayTaskLoop(); }, kDisplayTaskPriority);
}

void StateMachine::stopDisplayTask() {
  if (!displayTask_.isRunning()) {
    return;
  }

  if (!displayTask_.stop(kTransitionTimeoutMs)) {
    Serial.println("[SM] WARNING: Display task did not stop within timeout");
  }
}

void StateMachine::displayTaskLoop() {
  while (!displayTask_.shouldStop()) {
    if (renderRequested_.load(std::memory_order_acquire)) {
      renderRequested_.store(false, std::memory_order_release);

      // Take mutex for render operation
      ScopedMutex lock(renderMutex_);
      if (lock && current_ && corePtr_) {
        current_->render(*corePtr_);
      }
    }

    vTaskDelay(kRenderPollInterval);
  }
}

bool StateMachine::takeRenderLock(TickType_t timeout) {
  return xSemaphoreTake(renderMutex_, timeout) == pdTRUE;
}

void StateMachine::releaseRenderLock() {
  xSemaphoreGive(renderMutex_);
}

}  // namespace papyrix
