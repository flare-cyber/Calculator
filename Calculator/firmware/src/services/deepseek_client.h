#pragma once
// ===========================================================================
//  services/deepseek_client.h -- streaming SSE chat client for DeepSeek.
//
//  Blocking by design: it runs inside netTask only. Tokens are handed to the
//  caller one at a time through AiSink so the UI can render them as they
//  arrive.
// ===========================================================================
#include <stddef.h>
#include <stdint.h>

struct AiSink {
  // Called for every non-empty delta chunk (NUL-terminated, transient).
  void (*onToken)(const char* token, void* ctx);
  // Called exactly once at the end. code == 0 means success, positive values
  // are HTTP statuses, negative values are transport/client errors.
  void (*onStatus)(int code, const char* message, void* ctx);
  void* ctx;
};

class DeepSeekClient {
 public:
  DeepSeekClient();

  void begin(const char* apiKey);
  bool hasApiKey() const;

  // Perform one streaming chat completion. `cancel` may point at a volatile
  // flag that is polled between TLS reads so the UI can abort mid-stream.
  // Returns true on a clean end (including the [DONE] sentinel).
  bool chat(const char* prompt, const AiSink& sink, volatile bool* cancel);

  int lastHttpStatus() const { return lastHttpStatus_; }
  size_t lastReplyBytes() const { return replyBytes_; }

 private:
  const char* apiKey_;
  int lastHttpStatus_;
  size_t replyBytes_;
};
