#pragma once
#include <EpdFontFamily.h>
#include <SdFat.h>

#include <memory>
#include <string>
#include <vector>

#include "Block.h"

// represents a block of words in the html document
class TextBlock final : public Block {
 public:
  enum BLOCK_STYLE : uint8_t {
    JUSTIFIED = 0,
    LEFT_ALIGN = 1,
    CENTER_ALIGN = 2,
    RIGHT_ALIGN = 3,
  };

  // Flags stored in high bits of serialized style byte
  static constexpr uint8_t FLAG_MONOSPACE = 0x04;

  struct WordData {
    std::string word;
    uint16_t xPos;
    EpdFontFamily::Style style;
  };

 private:
  std::vector<WordData> wordData;
  BLOCK_STYLE style;
  bool useMonospace = false;

 public:
  explicit TextBlock(std::vector<WordData> data, const BLOCK_STYLE style, const bool useMonospace = false)
      : wordData(std::move(data)), style(style), useMonospace(useMonospace) {}
  ~TextBlock() override = default;
  void setStyle(const BLOCK_STYLE style) { this->style = style; }
  BLOCK_STYLE getStyle() const { return style; }
  void setUseMonospace(const bool mono) { useMonospace = mono; }
  bool getUseMonospace() const { return useMonospace; }
  bool isEmpty() override { return wordData.empty(); }
  void layout(GfxRenderer& renderer) override {};
  // given a renderer works out where to break the words into lines
  // monoFontId is used when useMonospace is true (0 = use regular fontId)
  void render(const GfxRenderer& renderer, int fontId, int x, int y, bool black = true, int monoFontId = 0) const;
  BlockType getType() override { return TEXT_BLOCK; }
  bool serialize(FsFile& file) const;
  static std::unique_ptr<TextBlock> deserialize(FsFile& file);
};
