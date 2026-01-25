#pragma once

#include <functional>
#include <memory>

class Page;
class GfxRenderer;
struct RenderConfig;

/**
 * Abstract interface for content parsers.
 * Implementations parse content (EPUB HTML, TXT, Markdown) into Page objects.
 */
class ContentParser {
 public:
  virtual ~ContentParser() = default;

  /**
   * Parse content and emit pages via callback.
   * @param onPageComplete Called for each completed page
   * @param maxPages Maximum pages to parse (0 = unlimited)
   * @return true if parsing completed successfully (may be partial if maxPages hit)
   */
  virtual bool parsePages(const std::function<void(std::unique_ptr<Page>)>& onPageComplete, uint16_t maxPages = 0) = 0;

  /**
   * Check if there's more content to parse after a partial parse.
   * @return true if more content available
   */
  virtual bool hasMoreContent() const = 0;

  /**
   * Reset parser to start from beginning.
   * Call this before re-parsing to extend cache.
   */
  virtual void reset() = 0;
};
