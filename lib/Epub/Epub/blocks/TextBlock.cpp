#include "TextBlock.h"

#include <GfxRenderer.h>
#include <Serialization.h>

void TextBlock::render(const GfxRenderer& renderer, const int fontId, const int x, const int y,
                       const bool black) const {
  for (const auto& wd : wordData) {
    renderer.drawText(fontId, wd.xPos + x, y, wd.word.c_str(), black, wd.style);
  }
}

bool TextBlock::serialize(FsFile& file) const {
  // Word count
  serialization::writePod(file, static_cast<uint16_t>(wordData.size()));

  // Write words, then xpos, then styles (maintains backward-compatible format)
  for (const auto& wd : wordData) serialization::writeString(file, wd.word);
  for (const auto& wd : wordData) serialization::writePod(file, wd.xPos);
  for (const auto& wd : wordData) serialization::writePod(file, wd.style);

  // Block style
  serialization::writePod(file, style);

  return true;
}

std::unique_ptr<TextBlock> TextBlock::deserialize(FsFile& file) {
  uint16_t wc;
  BLOCK_STYLE style;

  // Word count
  serialization::readPod(file, wc);

  // Sanity check: prevent allocation of unreasonably large vectors (max 10000 words per block)
  if (wc > 10000) {
    Serial.printf("[%lu] [TXB] Deserialization failed: word count %u exceeds maximum\n", millis(), wc);
    return nullptr;
  }

  // Read into temporary arrays (backward-compatible format: words, then xpos, then styles)
  std::vector<std::string> words(wc);
  std::vector<uint16_t> xpos(wc);
  std::vector<EpdFontFamily::Style> styles(wc);

  for (auto& w : words) serialization::readString(file, w);
  for (auto& x : xpos) serialization::readPod(file, x);
  for (auto& s : styles) serialization::readPod(file, s);

  // Block style
  serialization::readPod(file, style);

  // Combine into WordData vector
  std::vector<WordData> data;
  data.reserve(wc);
  for (uint16_t i = 0; i < wc; ++i) {
    data.push_back({std::move(words[i]), xpos[i], styles[i]});
  }

  return std::unique_ptr<TextBlock>(new TextBlock(std::move(data), style));
}
