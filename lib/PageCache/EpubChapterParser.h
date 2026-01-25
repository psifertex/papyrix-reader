#pragma once

#include <Epub.h>
#include <Epub/RenderConfig.h>

#include <memory>
#include <string>

#include "ContentParser.h"

class GfxRenderer;

/**
 * Content parser for EPUB chapters.
 * Wraps ChapterHtmlSlimParser to implement ContentParser interface.
 */
class EpubChapterParser : public ContentParser {
  std::shared_ptr<Epub> epub_;
  int spineIndex_;
  GfxRenderer& renderer_;
  RenderConfig config_;
  std::string imageCachePath_;
  bool hasMore_ = true;

 public:
  EpubChapterParser(std::shared_ptr<Epub> epub, int spineIndex, GfxRenderer& renderer, const RenderConfig& config,
                    const std::string& imageCachePath = "");
  ~EpubChapterParser() override = default;

  bool parsePages(const std::function<void(std::unique_ptr<Page>)>& onPageComplete, uint16_t maxPages = 0) override;
  bool hasMoreContent() const override { return hasMore_; }
  void reset() override;
};
