#include "TextBlock.h"

#include <GfxRenderer.h>
#include <Serialization.h>

void TextBlock::render(const GfxRenderer& renderer, const int fontId, const int x, const int y, const bool black,
                       const int monoFontId) const {
  const int effectiveFontId = (useMonospace && monoFontId != 0) ? monoFontId : fontId;
  for (const auto& wd : wordData) {
    renderer.drawText(effectiveFontId, wd.xPos + x, y, wd.word.c_str(), black, wd.style);
  }
}

bool TextBlock::serialize(FsFile& file) const {
  // Word count
  serialization::writePod(file, static_cast<uint16_t>(wordData.size()));

  // Write words, then xpos, then styles (maintains backward-compatible format)
  for (const auto& wd : wordData) serialization::writeString(file, wd.word);
  for (const auto& wd : wordData) serialization::writePod(file, wd.xPos);
  for (const auto& wd : wordData) serialization::writePod(file, wd.style);

  // Block style with flags in high bits (backward compatible: old readers ignore high bits)
  const uint8_t styleByte = static_cast<uint8_t>(style) | (useMonospace ? FLAG_MONOSPACE : 0);
  serialization::writePod(file, styleByte);

  return true;
}

std::unique_ptr<TextBlock> TextBlock::deserialize(FsFile& file) {
  uint16_t wc;

  // Word count
  if (!serialization::readPodChecked(file, wc)) {
    return nullptr;
  }

  // Sanity check: prevent allocation of unreasonably large vectors (max 10000 words per block)
  if (wc > 10000) {
    Serial.printf("[%lu] [TXB] Deserialization failed: word count %u exceeds maximum\n", millis(), wc);
    return nullptr;
  }

  // Read into temporary arrays (backward-compatible format: words, then xpos, then styles)
  std::vector<std::string> words(wc);
  std::vector<uint16_t> xpos(wc);
  std::vector<EpdFontFamily::Style> styles(wc);

  for (auto& w : words) {
    if (!serialization::readString(file, w)) {
      return nullptr;
    }
  }
  for (auto& x : xpos) {
    if (!serialization::readPodChecked(file, x)) {
      return nullptr;
    }
  }
  for (auto& s : styles) {
    if (!serialization::readPodChecked(file, s)) {
      return nullptr;
    }
  }

  // Block style with flags in high bits
  uint8_t styleByte;
  if (!serialization::readPodChecked(file, styleByte)) {
    return nullptr;
  }
  const BLOCK_STYLE style = static_cast<BLOCK_STYLE>(styleByte & 0x03);
  const bool useMonospace = (styleByte & FLAG_MONOSPACE) != 0;

  // Combine into WordData vector
  std::vector<WordData> data;
  data.reserve(wc);
  for (uint16_t i = 0; i < wc; ++i) {
    data.push_back({std::move(words[i]), xpos[i], styles[i]});
  }

  return std::unique_ptr<TextBlock>(new TextBlock(std::move(data), style, useMonospace));
}
