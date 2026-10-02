#include "util/sse_parser.h"

#include <string.h>

SseParser::SseParser()
    : len_(0), overflow_(false), sawDone_(false), droppedLines_(0), truncatedLines_(0) {
  line_[0] = '\0';
}

void SseParser::reset() {
  len_ = 0;
  overflow_ = false;
  sawDone_ = false;
  droppedLines_ = 0;
  truncatedLines_ = 0;
  line_[0] = '\0';
}

bool SseParser::processLine(char* line, char* out, size_t outCap) {
  // Empty line: event boundary, nothing to emit.
  if (line[0] == '\0') {
    return false;
  }

  // Comment / keep-alive. The SSE spec says lines beginning with ':' are
  // comments; DeepSeek sends `: keep-alive` to hold the connection open.
  if (line[0] == ':') {
    return false;
  }

  // We only care about `data:` lines. `event:` / `id:` / `retry:` are ignored.
  if (strncmp(line, "data:", 5) != 0) {
    return false;
  }

  if (overflow_) {
    ++truncatedLines_;
  }

  const char* payload = line + 5;

  // A single optional space after the colon is part of the framing.
  if (*payload == ' ') {
    ++payload;
  }

  if (out != nullptr && outCap > 0) {
    size_t n = strlen(payload);
    if (n >= outCap) {
      n = outCap - 1;
    }
    memcpy(out, payload, n);
    out[n] = '\0';
  }

  if (strcmp(payload, "[DONE]") == 0) {
    sawDone_ = true;
  }
  return true;
}

bool SseParser::feed(char c, char* out, size_t outCap) {
  if (c != '\n') {
    if (len_ < SSE_LINE_CAP - 1) {
      line_[len_++] = c;
    } else {
      // Line longer than the buffer: keep the prefix, remember the overflow.
      overflow_ = true;
      ++droppedLines_;
    }
    return false;
  }

  // End of line: terminate and strip an optional trailing CR.
  line_[len_] = '\0';
  if (len_ > 0 && line_[len_ - 1] == '\r') {
    line_[len_ - 1] = '\0';
  }

  bool emitted = processLine(line_, out, outCap);

  len_ = 0;
  overflow_ = false;
  line_[0] = '\0';
  return emitted;
}

void SseParser::feed(const char* data, size_t len, DataCallback cb, void* ctx) {
  if (data == nullptr || cb == nullptr) {
    return;
  }
  char payload[SSE_LINE_CAP];
  for (size_t i = 0; i < len; ++i) {
    if (feed(data[i], payload, sizeof(payload))) {
      cb(payload, ctx);
    }
  }
}
