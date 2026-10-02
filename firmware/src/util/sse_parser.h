#pragma once
// ===========================================================================
//  sse_parser.h -- incremental Server-Sent-Events line extractor.
//
//  Feed it one byte at a time (or a whole TLS read buffer). It assembles
//  lines, ignores blank lines and `: comment` / `: keep-alive` lines, and
//  hands back the payload of every `data:` line.
//
//  The DeepSeek streaming endpoint terminates the stream with `data: [DONE]`,
//  which is reported like any other data line so the caller can detect it.
//
//  One internal line buffer of SSE_LINE_CAP bytes is used; over-long lines are
//  truncated rather than overflowing.
// ===========================================================================
#include <stddef.h>

#include "config.h"

class SseParser {
 public:
  using DataCallback = void (*)(const char* data, void* ctx);

  SseParser();

  // Drop any partial line and clear the [DONE] latch.
  void reset();

  // Feed a single byte. Returns true and writes the NUL-terminated payload of
  // a completed `data:` line into `out` when one is available.
  bool feed(char c, char* out, size_t outCap);

  // Feed a buffer, invoking `cb` once per completed data line.
  void feed(const char* data, size_t len, DataCallback cb, void* ctx);

  bool sawDone() const { return sawDone_; }
  size_t droppedLines() const { return droppedLines_; }
  size_t truncatedLines() const { return truncatedLines_; }

 private:
  char line_[SSE_LINE_CAP];
  size_t len_;
  bool overflow_;
  bool sawDone_;
  size_t droppedLines_;
  size_t truncatedLines_;

  // Process a complete (NUL-terminated, CR-stripped) line. Returns true when
  // the line was a data line whose payload was copied into `out`.
  bool processLine(char* line, char* out, size_t outCap);
};
