#include "FileListState.h"

#include <Arduino.h>
#include <EInkDisplay.h>
#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <SDCardManager.h>
#include <esp_system.h>

#include <algorithm>
#include <cstring>

#include "../core/BootMode.h"
#include "../core/Core.h"
#include "../ui/Elements.h"
#include "MappedInputManager.h"
#include "ThemeManager.h"

namespace papyrix {

FileListState::FileListState(GfxRenderer& renderer)
    : renderer_(renderer),
      fileCount_(0),
      selectedIndex_(0),
      scrollOffset_(0),
      needsRender_(true),
      hasSelection_(false),
      goHome_(false),
      firstRender_(true) {
  strcpy(currentDir_, "/");
  selectedPath_[0] = '\0';
  memset(files_, 0, sizeof(files_));
}

FileListState::~FileListState() = default;

void FileListState::setDirectory(const char* dir) {
  if (dir && dir[0] != '\0') {
    strncpy(currentDir_, dir, sizeof(currentDir_) - 1);
    currentDir_[sizeof(currentDir_) - 1] = '\0';
  } else {
    strcpy(currentDir_, "/");
  }
}

void FileListState::enter(Core& core) {
  Serial.printf("[FILES] Entering, dir: %s\n", currentDir_);

  // Preserve position when returning from Reader via boot transition
  const auto& transition = getTransition();
  bool preservePosition = transition.isValid() && transition.returnTo == ReturnTo::FILE_MANAGER;

  if (!preservePosition) {
    selectedIndex_ = 0;
    scrollOffset_ = 0;
  }

  needsRender_ = true;
  hasSelection_ = false;
  goHome_ = false;
  firstRender_ = true;
  selectedPath_[0] = '\0';

  loadFiles(core);

  // Clamp selectedIndex to valid range after reloading files
  if (selectedIndex_ >= fileCount_) {
    selectedIndex_ = fileCount_ > 0 ? fileCount_ - 1 : 0;
  }
}

void FileListState::exit(Core& core) { Serial.println("[FILES] Exiting"); }

void FileListState::loadFiles(Core& core) {
  fileCount_ = 0;

  FsFile dir;
  auto result = core.storage.openDir(currentDir_, dir);
  if (!result.ok()) {
    Serial.printf("[FILES] Failed to open dir: %s\n", currentDir_);
    return;
  }

  // Temporary storage for sorting - use same fixed array
  uint16_t dirCount = 0;
  uint16_t filesStored = 0;               // Track files separately to avoid unsigned underflow
  uint16_t fileStartIdx = MAX_FILES / 2;  // Dirs at start, files at middle temporarily

  char name[256];
  FsFile entry;
  while ((entry = dir.openNextFile()) && (dirCount + filesStored < MAX_FILES)) {
    entry.getName(name, sizeof(name));

    if (isHidden(name)) {
      entry.close();
      continue;
    }

    bool isDir = entry.isDirectory();
    entry.close();

    if (isDir) {
      if (dirCount < fileStartIdx) {
        strncpy(files_[dirCount].name, name, MAX_NAME_LEN - 1);
        files_[dirCount].name[MAX_NAME_LEN - 1] = '\0';
        files_[dirCount].isDir = true;
        dirCount++;
      }
    } else if (isSupportedFile(name)) {
      uint16_t idx = fileStartIdx + filesStored;
      if (idx < MAX_FILES) {
        strncpy(files_[idx].name, name, MAX_NAME_LEN - 1);
        files_[idx].name[MAX_NAME_LEN - 1] = '\0';
        files_[idx].isDir = false;
        filesStored++;
      }
    }
  }
  dir.close();

  // Compact: move files right after directories
  for (uint16_t i = 0; i < filesStored && (dirCount + i) < MAX_FILES; i++) {
    files_[dirCount + i] = files_[fileStartIdx + i];
  }
  fileCount_ = dirCount + filesStored;

  // Simple insertion sort (small arrays, avoids heap)
  for (uint16_t i = 1; i < fileCount_; i++) {
    FileEntry temp = files_[i];
    int j = i - 1;

    // Sort: directories first, then alphabetically (case-insensitive)
    while (j >= 0) {
      bool swap = false;
      if (temp.isDir && !files_[j].isDir) {
        swap = true;
      } else if (temp.isDir == files_[j].isDir) {
        if (strcasecmp(temp.name, files_[j].name) < 0) {
          swap = true;
        }
      }

      if (swap) {
        files_[j + 1] = files_[j];
        j--;
      } else {
        break;
      }
    }
    files_[j + 1] = temp;
  }

  Serial.printf("[FILES] Loaded %u entries\n", fileCount_);
}

bool FileListState::isHidden(const char* name) const {
  if (name[0] == '.') return true;
  if (FsHelpers::isHiddenFsItem(name)) return true;
  if (strncmp(name, "FOUND.", 6) == 0) return true;
  return false;
}

bool FileListState::isSupportedFile(const char* name) const {
  const char* ext = strrchr(name, '.');
  if (!ext) return false;
  ext++;  // Skip the dot

  // Case-insensitive extension check (matches ContentTypes.cpp)
  if (strcasecmp(ext, "epub") == 0) return true;
  if (strcasecmp(ext, "xtc") == 0) return true;
  if (strcasecmp(ext, "xtch") == 0) return true;
  if (strcasecmp(ext, "xtg") == 0) return true;
  if (strcasecmp(ext, "xth") == 0) return true;
  if (strcasecmp(ext, "txt") == 0) return true;
  if (strcasecmp(ext, "md") == 0) return true;
  if (strcasecmp(ext, "markdown") == 0) return true;
  return false;
}

StateTransition FileListState::update(Core& core) {
  // Process input events
  Event e;
  while (core.events.pop(e)) {
    switch (e.type) {
      case EventType::ButtonPress:
        switch (e.button) {
          case Button::Up:
            navigateUp(core);
            break;
          case Button::Down:
            navigateDown(core);
            break;
          case Button::Left:
            pageUp(core);
            break;
          case Button::Right:
            pageDown(core);
            break;
          case Button::Center:
            openSelected(core);
            break;
          case Button::Back:
            goBack(core);
            break;
          case Button::Power:
            break;
        }
        break;

      default:
        break;
    }
  }

  // If a file was selected, transition to reader
  if (hasSelection_) {
    hasSelection_ = false;
    return StateTransition::to(StateId::Reader);
  }

  // Return to home if requested
  if (goHome_) {
    goHome_ = false;
    strcpy(currentDir_, "/");  // Reset for next entry
    return StateTransition::to(StateId::Home);
  }

  return StateTransition::stay(StateId::FileList);
}

void FileListState::render(Core& core) {
  if (!needsRender_) {
    return;
  }

  Theme& theme = THEME_MANAGER.mutableCurrent();

  renderer_.clearScreen(theme.backgroundColor);

  // Title
  renderer_.drawCenteredText(theme.readerFontId, 10, "Books", theme.primaryTextBlack, BOLD);

  // Empty state
  if (fileCount_ == 0) {
    renderer_.drawText(theme.uiFontId, 20, 60, "No books found", theme.primaryTextBlack);
    renderer_.displayBuffer();
    needsRender_ = false;
    core.display.markDirty();
    return;
  }

  // Calculate visible count dynamically (single-line items)
  constexpr int listStartY = 60;
  const int itemHeight = theme.itemHeight + theme.itemSpacing;
  const int visibleCount = getVisibleCount();

  // Adjust scroll to keep selection visible
  ensureVisible(visibleCount);

  // Draw file entries (single line each, truncated)
  const int end = std::min(static_cast<int>(scrollOffset_ + visibleCount), static_cast<int>(fileCount_));
  for (int i = scrollOffset_; i < end; i++) {
    const int y = listStartY + (i - scrollOffset_) * itemHeight;
    ui::fileEntry(renderer_, theme, y, files_[i].name, files_[i].isDir, i == selectedIndex_);
  }

  // Button hints - "Home" if at root, "Back" if in subfolder
  const char* backLabel = isAtRoot() ? "Home" : "Back";
  renderer_.drawButtonHints(theme.uiFontId, backLabel, "Open", "", "", theme.primaryTextBlack);

  if (firstRender_) {
    renderer_.displayBuffer(EInkDisplay::HALF_REFRESH);
    firstRender_ = false;
  } else {
    renderer_.displayBuffer();
  }
  needsRender_ = false;
  core.display.markDirty();
}

void FileListState::navigateUp(Core& core) {
  if (fileCount_ == 0) return;
  if (selectedIndex_ > 0) {
    selectedIndex_--;
  } else {
    selectedIndex_ = fileCount_ - 1;  // Wrap to last item
  }
  needsRender_ = true;
}

void FileListState::navigateDown(Core& core) {
  if (fileCount_ == 0) return;
  if (selectedIndex_ + 1 < fileCount_) {
    selectedIndex_++;
  } else {
    selectedIndex_ = 0;  // Wrap to first item
  }
  needsRender_ = true;
}

void FileListState::pageUp(Core& core) {
  if (fileCount_ == 0) return;
  // Fast scroll: jump by half visible count with circular wrap
  const int jumpSize = std::max(1, getVisibleCount() / 2);

  if (selectedIndex_ >= static_cast<uint16_t>(jumpSize)) {
    selectedIndex_ -= jumpSize;
  } else {
    // Wrap to end with circular offset
    int offset = jumpSize - static_cast<int>(selectedIndex_);
    if (offset >= static_cast<int>(fileCount_)) {
      selectedIndex_ = fileCount_ - 1;
    } else {
      selectedIndex_ = fileCount_ - offset;
    }
  }
  needsRender_ = true;
}

void FileListState::pageDown(Core& core) {
  if (fileCount_ == 0) return;
  // Fast scroll: jump by half visible count with circular wrap
  const int jumpSize = std::max(1, getVisibleCount() / 2);

  selectedIndex_ += jumpSize;
  if (selectedIndex_ >= fileCount_) {
    // Wrap to beginning with circular offset
    selectedIndex_ = selectedIndex_ - fileCount_;
    if (selectedIndex_ >= fileCount_) {
      selectedIndex_ = 0;
    }
  }
  needsRender_ = true;
}

void FileListState::openSelected(Core& core) {
  if (fileCount_ == 0) {
    return;
  }

  const FileEntry& entry = files_[selectedIndex_];

  // Build full path
  size_t dirLen = strlen(currentDir_);
  if (currentDir_[dirLen - 1] == '/') {
    snprintf(selectedPath_, sizeof(selectedPath_), "%s%s", currentDir_, entry.name);
  } else {
    snprintf(selectedPath_, sizeof(selectedPath_), "%s/%s", currentDir_, entry.name);
  }

  if (entry.isDir) {
    // Enter directory
    strncpy(currentDir_, selectedPath_, sizeof(currentDir_) - 1);
    currentDir_[sizeof(currentDir_) - 1] = '\0';
    selectedIndex_ = 0;
    loadFiles(core);
    needsRender_ = true;
  } else {
    // Select file - transition to Reader mode via restart
    Serial.printf("[FILES] Selected: %s\n", selectedPath_);
    showTransitionNotification("Opening book...");
    saveTransition(BootMode::READER, selectedPath_, ReturnTo::FILE_MANAGER);
    vTaskDelay(50 / portTICK_PERIOD_MS);
    ESP.restart();
  }
}

void FileListState::goBack(Core& core) {
  // Navigate to parent directory or go home if at root
  if (strcmp(currentDir_, "/") == 0) {
    // At root - go back to Home
    goHome_ = true;
    return;
  }

  // Find last slash and truncate
  char* lastSlash = strrchr(currentDir_, '/');
  if (lastSlash && lastSlash != currentDir_) {
    *lastSlash = '\0';
  } else {
    strcpy(currentDir_, "/");
  }

  selectedIndex_ = 0;
  loadFiles(core);
  needsRender_ = true;
}

void FileListState::ensureVisible(int visibleCount) {
  if (fileCount_ == 0 || visibleCount <= 0) return;
  if (selectedIndex_ < scrollOffset_) {
    scrollOffset_ = selectedIndex_;
  } else if (selectedIndex_ >= scrollOffset_ + visibleCount) {
    scrollOffset_ = selectedIndex_ - visibleCount + 1;
  }
}

int FileListState::getVisibleCount() const {
  const Theme& theme = THEME_MANAGER.current();
  constexpr int listStartY = 60;
  constexpr int bottomMargin = 70;
  const int availableHeight = renderer_.getScreenHeight() - listStartY - bottomMargin;
  const int itemHeight = theme.itemHeight + theme.itemSpacing;
  return availableHeight / itemHeight;
}

}  // namespace papyrix
