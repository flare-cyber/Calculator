#pragma once
// ===========================================================================
//  ui/renderer.h -- layout helpers shared by the UI screens.
// ===========================================================================
#include <stddef.h>
#include <stdint.h>

#include "hal/display.h"

namespace renderer {

// Number of visual lines `text` occupies when wrapped at `cols` characters.
// '\n' forces a line break. Always >= 1 (even for an empty string).
int wrapCount(const char* text, size_t cols);

// Draw wrapped text and return the number of lines actually rendered.
//  d             : display
//  text          : NUL-terminated source
//  x, firstBaseline : pixel origin of the first line
//  lineHeight    : vertical pitch between lines
//  maxRows       : number of lines that fit on screen
//  cols          : wrap width in characters
//  scroll        : wrapped lines to skip from the top
int drawWrapped(Display& d, const char* text, uint8_t x, uint8_t firstBaseline,
                uint8_t lineHeight, uint8_t maxRows, size_t cols, int scroll);

// Draw "LABEL text" with the label reserved for the first `labelCols` cells.
void drawLabelledInput(Display& d, uint8_t baseline, const char* label,
                       const char* text, bool showCaret, size_t cols);

// Horizontal scroll of a text field so the caret stays visible.
void drawScrolledField(Display& d, uint8_t x, uint8_t baseline, const char* text,
                       size_t cols, bool showCaret);

}  // namespace renderer
