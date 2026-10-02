#include "services/deepseek_client.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <esp_arduino_version.h>

#include <cstdio>
#include <cstring>
#include <type_traits>
#include <utility>

#include "config.h"
#include "secrets.h"
#include "deepseek_roots.h"
#include "util/sse_parser.h"

// The secure client was renamed in Arduino-ESP32 3.x (WiFiClientSecure ->
// NetworkClientSecure). Select the right header/type for the installed core.
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
#include <NetworkClientSecure.h>
using SecureClient = NetworkClientSecure;
#else
#include <WiFiClientSecure.h>
using SecureClient = WiFiClientSecure;
#endif

// ===========================================================================
//  TLS configuration
// ===========================================================================
namespace {

// Some Arduino-ESP32 cores expose WiFiClient::setBufferSizes(), others (for
// example the 2.0.x line used here) do not. Detect it at compile time so the
// call is used when available and skipped otherwise.
template <typename T, typename = void>
struct HasSetBufferSizes : std::false_type {};
template <typename T>
struct HasSetBufferSizes<
    T, decltype(std::declval<T&>().setBufferSizes(size_t(1), size_t(1)), void())>
    : std::true_type {};

template <typename T>
typename std::enable_if<HasSetBufferSizes<T>::value>::type
trySetBufferSizes(T& client) {
  client.setBufferSizes(2048, 1024);
}

template <typename T>
typename std::enable_if<!HasSetBufferSizes<T>::value>::type
trySetBufferSizes(T&) {}

void configureTls(SecureClient& client) {
#if ALLOW_INSECURE_TLS
  // Bring-up only: accepts any certificate. NEVER ship this enabled.
  client.setInsecure();
  Serial.println("[tls] WARNING: certificate validation disabled (ALLOW_INSECURE_TLS)");
#elif TLS_MODE == TLS_MODE_BUNDLE
  // Requires board_build.embed_files = data/cert/x509_crt_bundle.bin
  extern const uint8_t rootca_crt_bundle_start[] asm("_binary_data_cert_x509_crt_bundle_bin_start");
  client.setCACertBundle(rootca_crt_bundle_start);
#else
  // Default: pin the Amazon Root CA 1 that signs api.deepseek.com.
  client.setCACert(DEEPSEEK_ROOT_CA_PEM);
#endif
}

// ---------------------------------------------------------------------------
//  BodyReader -- yields decoded HTTP body bytes.
//
//  DeepSeek streams with `Transfer-Encoding: chunked`, and HTTPClient does not
//  de-chunk the raw stream returned by getStreamPtr(), so we do it here. Both
//  chunked and identity bodies are supported, with stall detection.
// ---------------------------------------------------------------------------
class BodyReader {
 public:
  BodyReader(Client* stream, bool chunked, uint32_t stallMs)
      : stream_(stream), chunked_(chunked), stallMs_(stallMs) {}

  // 1 = byte produced, 0 = clean EOF, -1 = error/timeout.
  int read(uint8_t* out) {
    if (eof_) {
      return 0;
    }
    if (!chunked_) {
      const int r = readRaw(out);
      if (r <= 0) {
        eof_ = true;
      }
      return r;
    }

    if (chunkRemaining_ == 0) {
      char line[24];
      const int n = readLine(line, sizeof(line));
      if (n < 0) {
        return -1;
      }
      if (n == 0) {
        eof_ = true;
        return 0;
      }

      unsigned long size = 0;
      int digits = 0;
      for (char* p = line; *p != '\0'; ++p) {
        const char c = *p;
        if (c == ';') {
          break;  // chunk extension
        }
        int v;
        if (c >= '0' && c <= '9') {
          v = c - '0';
        } else if (c >= 'a' && c <= 'f') {
          v = c - 'a' + 10;
        } else if (c >= 'A' && c <= 'F') {
          v = c - 'A' + 10;
        } else if (c == '\r' || c == ' ') {
          continue;
        } else {
          break;
        }
        size = size * 16 + static_cast<unsigned long>(v);
        ++digits;
      }
      if (digits == 0) {
        return -1;  // malformed chunk header
      }
      if (size == 0) {
        eof_ = true;  // final chunk
        return 0;
      }
      chunkRemaining_ = static_cast<size_t>(size);
    }

    const int r = readRaw(out);
    if (r <= 0) {
      eof_ = true;
      return r;
    }
    --chunkRemaining_;
    if (chunkRemaining_ == 0) {
      // Consume the CRLF that terminates the chunk payload.
      char trailer[2];
      readLine(trailer, sizeof(trailer));
    }
    return 1;
  }

 private:
  int readRaw(uint8_t* out) {
    if (stream_ == nullptr) {
      return -1;
    }
    const uint32_t start = millis();
    for (;;) {
      const int avail = stream_->available();
      if (avail > 0) {
        if (stream_->read(out, 1) == 1) {
          return 1;
        }
        return -1;
      }
      if (!stream_->connected()) {
        return 0;
      }
      if ((millis() - start) > stallMs_) {
        return -1;  // stalled
      }
      delay(2);
    }
  }

  int readLine(char* buf, size_t cap) {
    size_t n = 0;
    for (;;) {
      uint8_t c = 0;
      const int r = readRaw(&c);
      if (r < 0) {
        return -1;
      }
      if (r == 0) {
        if (n == 0) {
          return 0;
        }
        break;
      }
      if (c == '\n') {
        break;
      }
      if (c == '\r') {
        continue;
      }
      if (n + 1 < cap) {
        buf[n++] = static_cast<char>(c);
      }
    }
    buf[n] = '\0';
    return static_cast<int>(n);
  }

  Client* stream_;
  bool chunked_;
  uint32_t stallMs_;
  size_t chunkRemaining_ = 0;
  bool eof_ = false;
};

}  // namespace

// ===========================================================================
//  DeepSeekClient
// ===========================================================================

DeepSeekClient::DeepSeekClient()
    : apiKey_(nullptr), lastHttpStatus_(0), replyBytes_(0) {}

void DeepSeekClient::begin(const char* apiKey) { apiKey_ = apiKey; }

bool DeepSeekClient::hasApiKey() const {
  return apiKey_ != nullptr && apiKey_[0] != '\0' && strncmp(apiKey_, "sk-REPLACE", 10) != 0;
}

bool DeepSeekClient::chat(const char* prompt, const AiSink& sink, volatile bool* cancel) {
  lastHttpStatus_ = 0;
  replyBytes_ = 0;

  if (!hasApiKey()) {
    if (sink.onStatus != nullptr) {
      sink.onStatus(-2, "missing/invalid DeepSeek API key", sink.ctx);
    }
    return false;
  }

  // -- build the request body into a fixed buffer -------------------------
  char body[HTTP_BODY_CAP];
  {
    JsonDocument doc;
    doc["model"] = DEEPSEEK_MODEL;
    doc["temperature"] = DEEPSEEK_TEMPERATURE;
    doc["max_tokens"] = DEEPSEEK_MAX_TOKENS;
    doc["stream"] = true;
    doc["thinking"]["type"] = "disabled";  // calculator-style fast answers

    JsonArray messages = doc["messages"].to<JsonArray>();
    JsonObject sys = messages.add<JsonObject>();
    sys["role"] = "system";
    sys["content"] = DEEPSEEK_SYSTEM_PROMPT;
    JsonObject usr = messages.add<JsonObject>();
    usr["role"] = "user";
    usr["content"] = (prompt != nullptr) ? prompt : "";

    const size_t n = serializeJson(doc, body, sizeof(body));
    if (n == 0 || n >= sizeof(body)) {
      if (sink.onStatus != nullptr) {
        sink.onStatus(-3, "request JSON too large", sink.ctx);
      }
      return false;
    }
  }

  // -- open the TLS session ----------------------------------------------
  SecureClient client;
  trySetBufferSizes(client);
  client.setHandshakeTimeout(15);
  configureTls(client);

  HTTPClient http;
  http.setTimeout(DEEPSEEK_HTTP_TIMEOUT_MS);
  http.setReuse(false);

  if (!http.begin(client, DEEPSEEK_HOST, DEEPSEEK_PORT, DEEPSEEK_PATH, /*https=*/true)) {
    if (sink.onStatus != nullptr) {
      sink.onStatus(-4, "http.begin failed", sink.ctx);
    }
    return false;
  }

  char auth[160];
  snprintf(auth, sizeof(auth), "Bearer %s", apiKey_);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Accept", "text/event-stream");
  http.addHeader("Authorization", auth);

  // Ask HTTPClient to retain the Transfer-Encoding response header; without
  // this `http.header()` returns an empty string and the chunked body would
  // be fed raw to the SSE parser.
  const char* kCollectHeaders[] = {"Transfer-Encoding"};
  http.collectHeaders(kCollectHeaders, 1);

  const int status = http.POST(reinterpret_cast<uint8_t*>(body), strlen(body));
  lastHttpStatus_ = status;

  if (status <= 0) {
    char msg[EVENT_TEXT_LEN];
    snprintf(msg, sizeof(msg), "POST failed (%d)", status);
    if (sink.onStatus != nullptr) {
      sink.onStatus(status, msg, sink.ctx);
    }
    http.end();
    return false;
  }

  if (status != 200) {
    // Read a short slice of the error body (401, 402, 429, 5xx ...).
    char err[160];
    size_t n = 0;
    Client* s = http.getStreamPtr();
    const uint32_t start = millis();
    while (s != nullptr && n + 1 < sizeof(err) && (millis() - start) < 600) {
      if (s->available() <= 0) {
        if (!s->connected()) {
          break;
        }
        delay(5);
        continue;
      }
      char c = static_cast<char>(s->read());
      if (c == '\n' || c == '\r' || c == '\t') {
        c = ' ';
      }
      err[n++] = c;
    }
    err[n] = '\0';
    if (n == 0) {
      snprintf(err, sizeof(err), "HTTP %d", status);
    }
    if (sink.onStatus != nullptr) {
      sink.onStatus(status, err, sink.ctx);
    }
    http.end();
    return false;
  }

  // -- stream the SSE response -------------------------------------------
  const String transferEncoding = http.header("Transfer-Encoding");
  const bool chunked = transferEncoding.indexOf("chunked") >= 0;

  BodyReader reader(http.getStreamPtr(), chunked, DEEPSEEK_STREAM_STALL_MS);
  SseParser parser;
  char line[SSE_LINE_CAP];

  JsonDocument filter;
  filter["choices"][0]["delta"]["content"] = true;
  JsonDocument doc;

  bool cancelled = false;
  bool sawDone = false;
  bool transportError = false;

  uint8_t byte = 0;
  for (;;) {
    if (cancel != nullptr && *cancel) {
      cancelled = true;
      break;
    }

    const int r = reader.read(&byte);
    if (r < 0) {
      transportError = true;
      break;
    }
    if (r == 0) {
      break;  // clean end of body
    }

    if (!parser.feed(static_cast<char>(byte), line, sizeof(line))) {
      continue;
    }
    if (strcmp(line, "[DONE]") == 0) {
      sawDone = true;
      break;
    }

    // Parse only choices[0].delta.content; everything else is filtered out.
    doc.clear();
    const DeserializationError err =
        deserializeJson(doc, line, DeserializationOption::Filter(filter));
    if (err) {
      continue;
    }
    const char* token = doc["choices"][0]["delta"]["content"];
    if (token != nullptr && token[0] != '\0') {
      replyBytes_ += strlen(token);
      if (sink.onToken != nullptr) {
        sink.onToken(token, sink.ctx);
      }
    }
  }

  http.end();

  if (cancelled) {
    if (sink.onStatus != nullptr) {
      sink.onStatus(-5, "cancelled", sink.ctx);
    }
    return false;
  }
  if (transportError) {
    if (sink.onStatus != nullptr) {
      sink.onStatus(-6, "stream stalled or connection lost", sink.ctx);
    }
    return false;
  }

  // A clean EOF that never saw `data: [DONE]` is an incomplete response.
  if (!sawDone) {
    if (sink.onStatus != nullptr) {
      sink.onStatus(-7, "stream ended without [DONE]", sink.ctx);
    }
    return false;
  }

  if (sink.onStatus != nullptr) {
    sink.onStatus(0, "done", sink.ctx);
  }
  return true;
}
