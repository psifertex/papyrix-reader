/**
 * MarkdownParser.h
 *
 * Markdown parser for Papyrix Reader using MD4C.
 * Implements ContentParser interface for integration with PageCache.
 */

#pragma once

#include <ContentParser.h>
#include <Epub/RenderConfig.h>

#include <functional>
#include <memory>
#include <string>

class Page;
class GfxRenderer;
class ParsedText;
class TextBlock;

constexpr int MAX_WORD_SIZE = 200;

/**
 * Content parser for Markdown files using MD4C.
 * Parses markdown syntax (headers, bold, italic, lists, etc.) into styled text.
 */
class MarkdownParser : public ContentParser {
  std::string filepath_;
  GfxRenderer& renderer_;
  RenderConfig config_;
  size_t fileSize_ = 0;
  size_t currentOffset_ = 0;
  bool hasMore_ = true;

  // Parsing state
  int boldDepth_ = 0;
  int italicDepth_ = 0;
  int headerLevel_ = 0;
  bool inListItem_ = false;
  bool firstListItemWord_ = false;

  // Word buffer
  char partWordBuffer_[MAX_WORD_SIZE + 1] = {};
  int partWordBufferIndex_ = 0;

  // Current text block and page being built
  std::unique_ptr<ParsedText> currentTextBlock_;
  std::unique_ptr<Page> currentPage_;
  int16_t currentPageNextY_ = 0;

  // Page emission state for partial parsing
  std::function<void(std::unique_ptr<Page>)> onPageComplete_;
  uint16_t maxPages_ = 0;
  uint16_t pagesCreated_ = 0;
  bool hitMaxPages_ = false;

  // MD4C callbacks
  static int enterBlockCallback(int blockType, void* detail, void* userdata);
  static int leaveBlockCallback(int blockType, void* detail, void* userdata);
  static int enterSpanCallback(int spanType, void* detail, void* userdata);
  static int leaveSpanCallback(int spanType, void* detail, void* userdata);
  static int textCallback(int textType, const char* text, unsigned size, void* userdata);

  // Internal helpers
  void startNewTextBlock(int style);
  void makePages();
  bool addLineToPage(std::shared_ptr<TextBlock> line);
  void flushPartWordBuffer();
  int getCurrentFontStyle() const;
  void resetParsingState();

 public:
  MarkdownParser(std::string filepath, GfxRenderer& renderer, const RenderConfig& config);
  ~MarkdownParser() override;

  bool parsePages(const std::function<void(std::unique_ptr<Page>)>& onPageComplete, uint16_t maxPages = 0) override;
  bool hasMoreContent() const override { return hasMore_; }
  void reset() override;
};
