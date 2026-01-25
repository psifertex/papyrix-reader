#include "HomeView.h"

#include <EpdFontFamily.h>

#include <algorithm>
#include <cstdio>

namespace ui {

// Static definitions for constexpr arrays
constexpr const char* const HomeView::MENU_ITEMS[];

void render(const GfxRenderer& r, const Theme& t, const HomeView& v) {
  // Only clear if no cover (HomeState handles clear when cover present)
  if (!v.hasCoverBmp) {
    r.clearScreen(t.backgroundColor);
  }

  const int pageWidth = r.getScreenWidth();
  const int pageHeight = r.getScreenHeight();

  // Title "Papyrix Reader" at top
  r.drawCenteredText(t.readerFontId, 10, "Papyrix Reader", t.primaryTextBlack, EpdFontFamily::BOLD);

  // Battery indicator - top right
  battery(r, t, pageWidth - 80, 10, v.batteryPercent);

  // Book card dimensions (60% width, centered)
  const auto card = CardDimensions::calculate(pageWidth, pageHeight);
  const int cardX = card.x;
  const int cardY = card.y;
  const int cardWidth = card.width;
  const int cardHeight = card.height;

  const bool cardSelected = (v.selected == 0) && v.hasBook;
  const bool hasCover = v.coverData != nullptr || v.hasCoverBmp;

  if (v.hasBook) {
    // Draw book card with selection border (skip if BMP cover present - HomeState drew it)
    if (!v.hasCoverBmp) {
      if (cardSelected) {
        // Filled when selected (no cover image case)
        if (!hasCover) {
          r.fillRect(cardX, cardY, cardWidth, cardHeight, t.primaryTextBlack);
        } else {
          r.drawRect(cardX, cardY, cardWidth, cardHeight, t.primaryTextBlack);
          r.drawRect(cardX + 1, cardY + 1, cardWidth - 2, cardHeight - 2, t.primaryTextBlack);
          r.drawRect(cardX + 2, cardY + 2, cardWidth - 4, cardHeight - 4, t.primaryTextBlack);
        }
      } else {
        r.drawRect(cardX, cardY, cardWidth, cardHeight, t.primaryTextBlack);
      }
    }

    // Draw cover image if available (in-memory version; BMP cover rendered by HomeState)
    if (v.coverData != nullptr && v.coverWidth > 0 && v.coverHeight > 0) {
      // Simple centered draw (scaling is complex for 1-bit)
      const int coverX = cardX + (cardWidth - v.coverWidth) / 2;
      const int coverY = cardY + 10;
      r.drawImage(v.coverData, coverX, coverY, v.coverWidth, v.coverHeight);
    }

    // Text color based on selection (inverted if selected and no cover)
    const bool textOnCard = (cardSelected && !hasCover) ? !t.primaryTextBlack : t.primaryTextBlack;

    // Title and author centered in card
    const int maxTextWidth = cardWidth - 40;
    const int titleLineHeight = r.getLineHeight(t.uiFontId);

    // Wrap title to max 3 lines
    const auto titleLines = r.wrapTextWithHyphenation(t.uiFontId, v.bookTitle, maxTextWidth, 3);

    // Text area boundaries (leaving space for bookmark and "Continue Reading")
    const int textAreaTop = cardY + 70;
    const int textAreaBottom = cardY + cardHeight - 50;

    // Calculate total text height
    int totalTextHeight = static_cast<int>(titleLines.size()) * titleLineHeight;
    if (v.bookAuthor[0] != '\0') {
      totalTextHeight += titleLineHeight * 3 / 2;  // Author line + spacing
    }

    // Calculate vertical position for text (centered in text area, clamped to top)
    int textY = textAreaTop + std::max(0, (textAreaBottom - textAreaTop - totalTextHeight) / 2);

    // Draw white background box with black border when cover is present
    if (hasCover) {
      // Calculate max text width for box sizing
      int maxLineWidth = 0;
      for (const auto& line : titleLines) {
        int lineWidth = r.getTextWidth(t.uiFontId, line.c_str());
        if (lineWidth > maxLineWidth) maxLineWidth = lineWidth;
      }
      if (v.bookAuthor[0] != '\0') {
        const auto truncAuthor = r.truncatedText(t.uiFontId, v.bookAuthor, maxTextWidth);
        int authorWidth = r.getTextWidth(t.uiFontId, truncAuthor.c_str());
        if (authorWidth > maxLineWidth) maxLineWidth = authorWidth;
      }

      constexpr int boxPadding = 8;
      int boxWidth = maxLineWidth + boxPadding * 2;
      int boxHeight = totalTextHeight + boxPadding * 2;
      int boxX = (pageWidth - boxWidth) / 2;
      int boxY = textY - boxPadding;

      // Draw white filled box with black border
      r.fillRect(boxX, boxY, boxWidth, boxHeight, !t.primaryTextBlack);
      r.drawRect(boxX, boxY, boxWidth, boxHeight, t.primaryTextBlack);
    }

    // Draw title lines centered
    for (const auto& line : titleLines) {
      const int lineWidth = r.getTextWidth(t.uiFontId, line.c_str());
      const int lineX = cardX + (cardWidth - lineWidth) / 2;
      r.drawText(t.uiFontId, lineX, textY, line.c_str(), textOnCard);
      textY += titleLineHeight;
    }

    // Draw author if available
    if (v.bookAuthor[0] != '\0') {
      textY += titleLineHeight / 2;  // Extra spacing before author
      const auto truncAuthor = r.truncatedText(t.uiFontId, v.bookAuthor, maxTextWidth);
      const int authorWidth = r.getTextWidth(t.uiFontId, truncAuthor.c_str());
      const int authorX = cardX + (cardWidth - authorWidth) / 2;
      r.drawText(t.uiFontId, authorX, textY, truncAuthor.c_str(), textOnCard);
    }

    // "Continue Reading" at bottom of card
    const char* continueText = "Continue Reading";
    const int continueWidth = r.getTextWidth(t.uiFontId, continueText);
    const int continueX = cardX + (cardWidth - continueWidth) / 2;
    const int continueY = cardY + cardHeight - 40;

    // Draw white background for "Continue Reading" when cover is present
    if (hasCover) {
      constexpr int continuePadding = 6;
      const int continueBoxWidth = continueWidth + continuePadding * 2;
      const int continueBoxHeight = titleLineHeight + continuePadding;
      const int continueBoxX = (pageWidth - continueBoxWidth) / 2;
      const int continueBoxY = continueY - continuePadding / 2;
      r.fillRect(continueBoxX, continueBoxY, continueBoxWidth, continueBoxHeight, !t.primaryTextBlack);
      r.drawRect(continueBoxX, continueBoxY, continueBoxWidth, continueBoxHeight, t.primaryTextBlack);
    }

    r.drawText(t.uiFontId, continueX, continueY, continueText, textOnCard);

  } else {
    // No book open - show bordered placeholder
    r.drawRect(cardX, cardY, cardWidth, cardHeight, t.primaryTextBlack);

    const char* noBookText = "No book open";
    const int noBookWidth = r.getTextWidth(t.uiFontId, noBookText);
    const int noBookX = cardX + (cardWidth - noBookWidth) / 2;
    const int noBookY = cardY + cardHeight / 2 - r.getFontAscenderSize(t.uiFontId) / 2;
    r.drawText(t.uiFontId, noBookX, noBookY, noBookText, t.primaryTextBlack);
  }

  // Grid 2x1 at bottom of page (Files, Settings)
  // Aligned with button hints positions
  constexpr int gridItemHeight = 50;
  constexpr int buttonHintsY = 50;  // Distance from bottom for button hints
  const int gridY = pageHeight - buttonHintsY - gridItemHeight - 10;

  // Grid positions matching button hint layout
  constexpr int gridPositions[] = {25, 245};
  constexpr int gridItemWidth = 211;

  for (int i = 0; i < HomeView::MENU_ITEM_COUNT; i++) {
    const int itemX = gridPositions[i];
    const bool isSelected = (v.selected == i + 1);  // +1 because 0 is book card

    if (isSelected) {
      r.fillRect(itemX, gridY, gridItemWidth, gridItemHeight, t.selectionFillBlack);
    } else {
      r.drawRect(itemX, gridY, gridItemWidth, gridItemHeight, t.primaryTextBlack);
    }

    // Draw centered text
    const bool itemTextColor = isSelected ? t.selectionTextBlack : t.primaryTextBlack;
    const int textWidth = r.getTextWidth(t.uiFontId, HomeView::MENU_ITEMS[i]);
    const int textX = itemX + (gridItemWidth - textWidth) / 2;
    const int textY = gridY + (gridItemHeight - r.getFontAscenderSize(t.uiFontId)) / 2;
    r.drawText(t.uiFontId, textX, textY, HomeView::MENU_ITEMS[i], itemTextColor);
  }

  // Button hints
  buttonBar(r, t, "", "Open", "Left", "Right");

  // Note: displayBuffer() is NOT called here; HomeState will call it
  // after rendering the cover image on top of the card area
}

void render(const GfxRenderer& r, const Theme& t, const FileListView& v) {
  r.clearScreen(t.backgroundColor);

  // Title with path
  title(r, t, t.screenMarginTop, "Files");

  // Current path (truncated if needed)
  const int pathY = 40;
  const int maxPathW = r.getScreenWidth() - 2 * t.screenMarginSide - 16;
  const auto truncPath = r.truncatedText(t.smallFontId, v.currentPath, maxPathW);
  r.drawText(t.smallFontId, t.screenMarginSide + 8, pathY, truncPath.c_str(), t.secondaryTextBlack);

  // File list
  const int listStartY = 65;
  const int pageStart = v.getPageStart();
  const int pageEnd = v.getPageEnd();

  for (int i = pageStart; i < pageEnd; i++) {
    const int y = listStartY + (i - pageStart) * (t.itemHeight + t.itemSpacing);
    fileEntry(r, t, y, v.files[i].name, v.files[i].isDirectory, i == v.selected);
  }

  // Page indicator
  if (v.getPageCount() > 1) {
    char pageStr[16];
    snprintf(pageStr, sizeof(pageStr), "%d/%d", v.page + 1, v.getPageCount());
    const int pageY = r.getScreenHeight() - 50;
    centeredText(r, t, pageY, pageStr);
  }

  buttonBar(r, t, "Back", "Open", "", "");

  r.displayBuffer();
}

void render(const GfxRenderer& r, const Theme& t, ChapterListView& v) {
  r.clearScreen(t.backgroundColor);

  title(r, t, t.screenMarginTop, "Chapters");

  constexpr int listStartY = 60;
  const int availableHeight = r.getScreenHeight() - listStartY - 50;
  const int itemHeight = t.itemHeight + t.itemSpacing;
  const int visibleCount = availableHeight / itemHeight;

  v.ensureVisible(visibleCount);

  const int end = std::min(v.scrollOffset + visibleCount, static_cast<int>(v.chapterCount));
  for (int i = v.scrollOffset; i < end; i++) {
    const int y = listStartY + (i - v.scrollOffset) * itemHeight;
    chapterItem(r, t, y, v.chapters[i].title, v.chapters[i].depth, i == v.selected, i == v.currentChapter);
  }

  buttonBar(r, t, "Back", "Go", "", "");

  r.displayBuffer();
}

}  // namespace ui
