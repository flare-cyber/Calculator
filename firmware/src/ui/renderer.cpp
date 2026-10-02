#include "ui/renderer.h"

#include <string.h>

namespace renderer {

int wrapCount(const char* text, size_t cols) {
  if (cols == 0) {
    return 1;
  }
  if (text == nullptr || *text == '\0') {
    return 1;
  }
  int lines = 1;
  size_t lineLen = 0;
  for (const char* p = text; *p != '\0'; ++p) {
    if (*p == '\n') {
      ++lines;
      lineLen = 0;
      continue;
    }
    ++lineLen;
    if (lineLen >= cols) {
      lineLen = 0;
      // Only count a new line when more content actually follows; otherwise
      // an exact multiple of `cols` would be reported one line too long,
      // which disagrees with drawWrapped()'s final-flush behaviour.
      if (p[1] != '\0') {
        ++lines;
      }
    }
  }
  return lines;
}

int drawWrapped(Display& d, const char* text, uint8_t x, uint8_t firstBaseline,
                uint8_t lineHeight, uint8_t maxRows, size_t cols, int scroll) {
  if (cols == 0 || maxRows == 0) {
    return 0;
  }
  if (scroll < 0) {
    scroll = 0;
  }
  if (text == nullptr) {
    text = "";
  }

  d.useSmallFont();

  char line[64];
  size_t lineLen = 0;
  int lineIndex = 0;
  int drawn = 0;

  auto flushLine = [&]() {
    line[lineLen] = '\0';
    if (lineIndex >= scroll && drawn < maxRows) {
      d.g().drawStr(x, static_cast<uint8_t>(firstBaseline + drawn * lineHeight), line);
      ++drawn;
    }
    ++lineIndex;
    lineLen = 0;
  };

  for (const char* p = text; *p != '\0'; ++p) {
    if (*p == '\n') {
      flushLine();
      continue;
    }
    if (lineLen + 1 >= sizeof(line)) {
      flushLine();
    }
    line[lineLen++] = *p;
    if (lineLen >= cols) {
      flushLine();
    }
  }
  if (lineLen > 0 || lineIndex == 0) {
    flushLine();
  }
  return drawn;
}

void drawScrolledField(Display& d, uint8_t x, uint8_t baseline, const char* text,
                       size_t cols, bool showCaret) {
  if (text == nullptr) {
    text = "";
  }
  d.useSmallFont();

  const size_t len = strlen(text);
  const size_t start = (len > cols) ? (len - cols) : 0;
  char view[64];
  size_t n = len - start;
  if (n >= sizeof(view)) {
    n = sizeof(view) - 1;
  }
  memcpy(view, text + start, n);
  view[n] = '\0';
  d.g().drawStr(x, baseline, view);

  if (showCaret) {
    const uint8_t caretX = static_cast<uint8_t>(x + d.g().getStrWidth(view));
    d.g().drawBox(caretX, static_cast<uint8_t>(baseline - 8), 5, 2);
  }
}

void drawLabelledInput(Display& d, uint8_t baseline, const char* label,
                       const char* text, bool showCaret, size_t cols) {
  d.useSmallFont();
  if (label != nullptr) {
    d.g().drawStr(0, baseline, label);
  }
  const uint8_t labelW = (label != nullptr) ? static_cast<uint8_t>(d.g().getStrWidth(label)) : 0;
  const size_t reserved = labelW / 6;  // 6 px per glyph in the small font
  const size_t fieldCols = (cols > reserved) ? (cols - reserved) : 1;
  drawScrolledField(d, labelW, baseline, text, fieldCols, showCaret);
}

}  // namespace renderer
