/**
 * MarkdownParser.cpp
 *
 * Markdown parser implementation using MD4C.
 */

#include "MarkdownParser.h"

#include <EpdFontFamily.h>
#include <Epub/Page.h>
#include <Epub/ParsedText.h>
#include <Epub/blocks/TextBlock.h>
#include <GfxRenderer.h>
#include <HardwareSerial.h>
#include <SDCardManager.h>

#include <utility>

#include "md4c/md4c.h"

namespace {
bool isWhitespaceChar(const char c) { return c == ' ' || c == '\r' || c == '\n' || c == '\t'; }
}  // namespace

MarkdownParser::MarkdownParser(std::string filepath, GfxRenderer& renderer, const RenderConfig& config)
    : filepath_(std::move(filepath)), renderer_(renderer), config_(config) {}

MarkdownParser::~MarkdownParser() = default;

void MarkdownParser::reset() {
  currentOffset_ = 0;
  hasMore_ = true;
}

void MarkdownParser::resetParsingState() {
  boldDepth_ = 0;
  italicDepth_ = 0;
  headerLevel_ = 0;
  inListItem_ = false;
  firstListItemWord_ = false;
  partWordBufferIndex_ = 0;
  currentTextBlock_.reset();
  currentPage_.reset();
  currentPageNextY_ = 0;
  pagesCreated_ = 0;
  hitMaxPages_ = false;
}

int MarkdownParser::getCurrentFontStyle() const {
  if (boldDepth_ > 0 && italicDepth_ > 0) {
    return EpdFontFamily::BOLD_ITALIC;
  } else if (boldDepth_ > 0) {
    return EpdFontFamily::BOLD;
  } else if (italicDepth_ > 0) {
    return EpdFontFamily::ITALIC;
  }
  return EpdFontFamily::REGULAR;
}

void MarkdownParser::flushPartWordBuffer() {
  if (partWordBufferIndex_ > 0) {
    partWordBuffer_[partWordBufferIndex_] = '\0';
    if (currentTextBlock_) {
      currentTextBlock_->addWord(partWordBuffer_, static_cast<EpdFontFamily::Style>(getCurrentFontStyle()));
    }
    partWordBufferIndex_ = 0;
  }
}

void MarkdownParser::startNewTextBlock(const int style) {
  if (currentTextBlock_) {
    // Already have a text block running and it is empty - just reuse it
    if (currentTextBlock_->isEmpty()) {
      currentTextBlock_->setStyle(static_cast<TextBlock::BLOCK_STYLE>(style));
      return;
    }

    makePages();
  }
  currentTextBlock_.reset(
      new ParsedText(static_cast<TextBlock::BLOCK_STYLE>(style), config_.indentLevel, config_.hyphenation));
}

bool MarkdownParser::addLineToPage(std::shared_ptr<TextBlock> line) {
  const int lineHeight = static_cast<int>(renderer_.getLineHeight(config_.fontId) * config_.lineCompression);

  if (currentPageNextY_ + lineHeight > config_.viewportHeight) {
    if (onPageComplete_) {
      onPageComplete_(std::move(currentPage_));
      pagesCreated_++;
    }
    currentPage_.reset(new Page());
    currentPageNextY_ = 0;

    if (maxPages_ > 0 && pagesCreated_ >= maxPages_) {
      hitMaxPages_ = true;
      return false;
    }
  }

  currentPage_->elements.push_back(std::make_shared<PageLine>(line, 0, currentPageNextY_));
  currentPageNextY_ += lineHeight;
  return true;
}

void MarkdownParser::makePages() {
  if (!currentTextBlock_) {
    return;
  }

  if (!currentPage_) {
    currentPage_.reset(new Page());
    currentPageNextY_ = 0;
  }

  const int lineHeight = static_cast<int>(renderer_.getLineHeight(config_.fontId) * config_.lineCompression);

  currentTextBlock_->layoutAndExtractLines(renderer_, config_.fontId, config_.viewportWidth,
                                           [this](const std::shared_ptr<TextBlock>& textBlock) {
                                             if (!hitMaxPages_) {
                                               addLineToPage(textBlock);
                                             }
                                           });

  // Extra paragraph spacing based on spacingLevel (0=none, 1=small, 3=large)
  switch (config_.spacingLevel) {
    case 1:
      currentPageNextY_ += lineHeight / 4;  // Small (1/4 line)
      break;
    case 3:
      currentPageNextY_ += lineHeight;  // Large (full line)
      break;
  }
}

int MarkdownParser::enterBlockCallback(int blockType, void* detail, void* userdata) {
  auto* self = static_cast<MarkdownParser*>(userdata);

  if (self->hitMaxPages_) return 1;  // Stop parsing

  switch (static_cast<MD_BLOCKTYPE>(blockType)) {
    case MD_BLOCK_DOC:
      // Start of document - initialize first text block
      self->startNewTextBlock(self->config_.paragraphAlignment);
      break;

    case MD_BLOCK_H: {
      // Flush any pending word
      self->flushPartWordBuffer();
      // Headers are centered and bold
      auto* h = static_cast<MD_BLOCK_H_DETAIL*>(detail);
      self->headerLevel_ = h->level;
      self->startNewTextBlock(TextBlock::CENTER_ALIGN);
      self->boldDepth_++;
      break;
    }

    case MD_BLOCK_P:
      // Flush any pending word
      self->flushPartWordBuffer();
      // Normal paragraph
      self->startNewTextBlock(self->config_.paragraphAlignment);
      break;

    case MD_BLOCK_QUOTE:
      // Blockquote - use italic for differentiation
      self->flushPartWordBuffer();
      self->startNewTextBlock(TextBlock::LEFT_ALIGN);
      self->italicDepth_++;
      break;

    case MD_BLOCK_UL:
    case MD_BLOCK_OL:
      // Lists - nothing special at list start
      break;

    case MD_BLOCK_LI:
      // List item - add bullet prefix
      self->flushPartWordBuffer();
      self->startNewTextBlock(TextBlock::LEFT_ALIGN);
      self->inListItem_ = true;
      self->firstListItemWord_ = true;
      break;

    case MD_BLOCK_CODE:
      // Code block - add placeholder
      self->flushPartWordBuffer();
      self->startNewTextBlock(TextBlock::LEFT_ALIGN);
      // Code blocks are rendered with a placeholder since we can't use monospace
      if (self->currentTextBlock_) {
        self->currentTextBlock_->addWord("[Code:", EpdFontFamily::ITALIC);
      }
      break;

    case MD_BLOCK_HR:
      // Horizontal rule - add visual separator
      self->flushPartWordBuffer();
      self->startNewTextBlock(TextBlock::CENTER_ALIGN);
      if (self->currentTextBlock_) {
        self->currentTextBlock_->addWord("───────────", EpdFontFamily::REGULAR);
      }
      break;

    case MD_BLOCK_TABLE:
      // Tables - add placeholder
      self->flushPartWordBuffer();
      self->startNewTextBlock(TextBlock::CENTER_ALIGN);
      if (self->currentTextBlock_) {
        self->currentTextBlock_->addWord("[Table", EpdFontFamily::ITALIC);
        self->currentTextBlock_->addWord("omitted]", EpdFontFamily::ITALIC);
      }
      break;

    case MD_BLOCK_HTML:
      // Raw HTML - skip
      break;

    default:
      break;
  }

  return 0;
}

int MarkdownParser::leaveBlockCallback(int blockType, void* detail, void* userdata) {
  auto* self = static_cast<MarkdownParser*>(userdata);
  (void)detail;

  if (self->hitMaxPages_) return 1;  // Stop parsing

  switch (static_cast<MD_BLOCKTYPE>(blockType)) {
    case MD_BLOCK_DOC:
      // End of document
      break;

    case MD_BLOCK_H:
      // End of header
      self->flushPartWordBuffer();
      if (self->boldDepth_ > 0) self->boldDepth_--;
      self->headerLevel_ = 0;
      break;

    case MD_BLOCK_P:
    case MD_BLOCK_LI:
      // End of paragraph or list item
      self->flushPartWordBuffer();
      self->inListItem_ = false;
      self->firstListItemWord_ = false;
      break;

    case MD_BLOCK_QUOTE:
      // End of blockquote
      self->flushPartWordBuffer();
      if (self->italicDepth_ > 0) self->italicDepth_--;
      break;

    case MD_BLOCK_CODE:
      // End of code block
      self->flushPartWordBuffer();
      if (self->currentTextBlock_) {
        self->currentTextBlock_->addWord("]", EpdFontFamily::ITALIC);
      }
      break;

    default:
      break;
  }

  return 0;
}

int MarkdownParser::enterSpanCallback(int spanType, void* detail, void* userdata) {
  auto* self = static_cast<MarkdownParser*>(userdata);
  (void)detail;

  if (self->hitMaxPages_) return 1;  // Stop parsing

  switch (static_cast<MD_SPANTYPE>(spanType)) {
    case MD_SPAN_STRONG:
      self->boldDepth_++;
      break;

    case MD_SPAN_EM:
      self->italicDepth_++;
      break;

    case MD_SPAN_CODE:
      // Inline code - use italic
      self->italicDepth_++;
      break;

    case MD_SPAN_A:
      // Links - underline not supported, just show text normally
      break;

    case MD_SPAN_IMG:
      // Images - add placeholder
      self->flushPartWordBuffer();
      if (self->currentTextBlock_) {
        self->currentTextBlock_->addWord("[Image]", EpdFontFamily::ITALIC);
      }
      break;

    case MD_SPAN_DEL:
      // Strikethrough - not supported, just show text
      break;

    default:
      break;
  }

  return 0;
}

int MarkdownParser::leaveSpanCallback(int spanType, void* detail, void* userdata) {
  auto* self = static_cast<MarkdownParser*>(userdata);
  (void)detail;

  if (self->hitMaxPages_) return 1;  // Stop parsing

  switch (static_cast<MD_SPANTYPE>(spanType)) {
    case MD_SPAN_STRONG:
      if (self->boldDepth_ > 0) self->boldDepth_--;
      break;

    case MD_SPAN_EM:
      if (self->italicDepth_ > 0) self->italicDepth_--;
      break;

    case MD_SPAN_CODE:
      if (self->italicDepth_ > 0) self->italicDepth_--;
      break;

    default:
      break;
  }

  return 0;
}

int MarkdownParser::textCallback(int textType, const char* text, unsigned size, void* userdata) {
  auto* self = static_cast<MarkdownParser*>(userdata);

  if (self->hitMaxPages_) return 1;  // Stop parsing

  // Handle special text types
  switch (static_cast<MD_TEXTTYPE>(textType)) {
    case MD_TEXT_BR:
    case MD_TEXT_SOFTBR:
      // Line break - flush current word and add space
      self->flushPartWordBuffer();
      return 0;

    case MD_TEXT_CODE:
      // Code text - just add ellipsis for code blocks
      if (self->currentTextBlock_) {
        self->currentTextBlock_->addWord("...", EpdFontFamily::ITALIC);
      }
      return 0;

    case MD_TEXT_HTML:
      // Raw HTML - skip
      return 0;

    case MD_TEXT_ENTITY:
      // HTML entities - try to handle common ones
      if (size == 6 && strncmp(text, "&nbsp;", 6) == 0) {
        self->flushPartWordBuffer();
      } else if (self->partWordBufferIndex_ < MAX_WORD_SIZE) {
        if (size == 6 && strncmp(text, "&quot;", 6) == 0) {
          self->partWordBuffer_[self->partWordBufferIndex_++] = '"';
        } else if (size == 5 && strncmp(text, "&amp;", 5) == 0) {
          self->partWordBuffer_[self->partWordBufferIndex_++] = '&';
        } else if (size == 4 && strncmp(text, "&lt;", 4) == 0) {
          self->partWordBuffer_[self->partWordBufferIndex_++] = '<';
        } else if (size == 4 && strncmp(text, "&gt;", 4) == 0) {
          self->partWordBuffer_[self->partWordBufferIndex_++] = '>';
        }
      }
      return 0;

    default:
      break;
  }

  // Add bullet for first word in list item
  if (self->firstListItemWord_ && self->inListItem_) {
    if (self->currentTextBlock_) {
      self->currentTextBlock_->addWord("•", EpdFontFamily::REGULAR);
    }
    self->firstListItemWord_ = false;
  }

  EpdFontFamily::Style fontStyle = static_cast<EpdFontFamily::Style>(self->getCurrentFontStyle());

  // Process text character by character
  for (unsigned i = 0; i < size; i++) {
    if (isWhitespaceChar(text[i])) {
      // Whitespace - flush word buffer
      if (self->partWordBufferIndex_ > 0) {
        self->partWordBuffer_[self->partWordBufferIndex_] = '\0';
        if (self->currentTextBlock_) {
          self->currentTextBlock_->addWord(self->partWordBuffer_, fontStyle);
        }
        self->partWordBufferIndex_ = 0;
      }
      continue;
    }

    // If buffer is full, flush it
    if (self->partWordBufferIndex_ >= MAX_WORD_SIZE) {
      self->partWordBuffer_[self->partWordBufferIndex_] = '\0';
      if (self->currentTextBlock_) {
        self->currentTextBlock_->addWord(self->partWordBuffer_, fontStyle);
      }
      self->partWordBufferIndex_ = 0;
    }

    self->partWordBuffer_[self->partWordBufferIndex_++] = text[i];
  }

  // If we have > 750 words buffered up, perform layout to free memory
  if (self->currentTextBlock_ && self->currentTextBlock_->size() > 750) {
    self->currentTextBlock_->layoutAndExtractLines(
        self->renderer_, self->config_.fontId, self->config_.viewportWidth,
        [self](const std::shared_ptr<TextBlock>& textBlock) {
          if (!self->hitMaxPages_) {
            self->addLineToPage(textBlock);
          }
        },
        false);
  }

  return 0;
}

bool MarkdownParser::parsePages(const std::function<void(std::unique_ptr<Page>)>& onPageComplete, uint16_t maxPages) {
  FsFile file;
  if (!SdMan.openFileForRead("MD", filepath_, file)) {
    Serial.printf("[MD] Failed to open file: %s\n", filepath_.c_str());
    return false;
  }

  fileSize_ = file.size();
  if (fileSize_ == 0) {
    Serial.printf("[MD] Empty markdown file\n");
    file.close();
    hasMore_ = false;
    return true;
  }

  // For now, we parse the entire file each time (markdown parsing is stateful)
  // TODO: Implement chunked parsing if needed for very large files

  // Allocate buffer to read entire file
  uint8_t* buffer = static_cast<uint8_t*>(malloc(fileSize_ + 1));
  if (!buffer) {
    Serial.printf("[MD] Failed to allocate buffer for %zu bytes\n", fileSize_);
    file.close();
    return false;
  }

  // Read file content
  size_t bytesRead = file.read(buffer, fileSize_);
  file.close();

  if (bytesRead != fileSize_) {
    Serial.printf("[MD] Only read %zu of %zu bytes\n", bytesRead, fileSize_);
    free(buffer);
    return false;
  }
  buffer[bytesRead] = '\0';

  Serial.printf("[MD] Read %zu bytes of markdown\n", bytesRead);

  // Initialize state
  resetParsingState();
  onPageComplete_ = onPageComplete;
  maxPages_ = maxPages;

  // Setup MD4C parser
  MD_PARSER parser = {};
  parser.abi_version = 0;
  parser.flags = MD_DIALECT_COMMONMARK;  // Use CommonMark for basic markdown
  parser.enter_block = reinterpret_cast<int (*)(MD_BLOCKTYPE, void*, void*)>(enterBlockCallback);
  parser.leave_block = reinterpret_cast<int (*)(MD_BLOCKTYPE, void*, void*)>(leaveBlockCallback);
  parser.enter_span = reinterpret_cast<int (*)(MD_SPANTYPE, void*, void*)>(enterSpanCallback);
  parser.leave_span = reinterpret_cast<int (*)(MD_SPANTYPE, void*, void*)>(leaveSpanCallback);
  parser.text = reinterpret_cast<int (*)(MD_TEXTTYPE, const MD_CHAR*, MD_SIZE, void*)>(textCallback);
  parser.debug_log = nullptr;
  parser.syntax = nullptr;

  // Parse markdown
  int result = md_parse(reinterpret_cast<const char*>(buffer), static_cast<unsigned>(bytesRead), &parser, this);

  free(buffer);

  // If we stopped due to hitMaxPages_, that's expected, not an error
  if (result != 0 && !hitMaxPages_) {
    Serial.printf("[MD] md_parse failed with code %d\n", result);
    return false;
  }

  // Process any remaining content
  flushPartWordBuffer();
  if (currentTextBlock_ && !currentTextBlock_->isEmpty()) {
    makePages();
  }
  if (currentPage_ && !currentPage_->elements.empty() && onPageComplete_) {
    onPageComplete_(std::move(currentPage_));
    pagesCreated_++;
  }

  // Determine if we have more content
  // For markdown, we parse all at once, so hasMore is based on hitMaxPages
  hasMore_ = hitMaxPages_;
  if (!hasMore_) {
    currentOffset_ = fileSize_;
  }

  Serial.printf("[MD] Parsed %d pages, hasMore=%d\n", pagesCreated_, hasMore_);
  return true;
}
