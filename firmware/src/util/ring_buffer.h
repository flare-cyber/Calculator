#pragma once
// ===========================================================================
//  util/ring_buffer.h -- fixed-capacity ring buffer, header only.
//
//  No dynamic allocation, no exceptions, safe for embedded use. `push`
//  rejects when full; `pushOverwrite` evicts the oldest element instead.
// ===========================================================================
#include <stddef.h>

template <typename T, size_t N>
class RingBuffer {
 public:
  RingBuffer() : head_(0), count_(0) {}

  void clear() {
    head_ = 0;
    count_ = 0;
  }

  size_t size() const { return count_; }
  constexpr size_t capacity() const { return N; }
  bool empty() const { return count_ == 0; }
  bool full() const { return count_ == N; }

  // Append a copy. Returns false (and does nothing) when the buffer is full.
  bool push(const T& value) {
    if (count_ == N) {
      return false;
    }
    buffer_[(head_ + count_) % N] = value;
    ++count_;
    return true;
  }

  // Append, evicting the oldest element when the buffer is full.
  void pushOverwrite(const T& value) {
    if (count_ == N) {
      buffer_[head_] = value;
      head_ = (head_ + 1) % N;
    } else {
      buffer_[(head_ + count_) % N] = value;
      ++count_;
    }
  }

  // Remove and return the oldest element.
  bool pop(T* out) {
    if (count_ == 0 || out == nullptr) {
      return false;
    }
    *out = buffer_[head_];
    head_ = (head_ + 1) % N;
    --count_;
    return true;
  }

  // Peek at the newest element.
  const T* back() const {
    if (count_ == 0) {
      return nullptr;
    }
    return &buffer_[(head_ + count_ - 1) % N];
  }

  // Index 0 is the oldest element.
  const T& operator[](size_t index) const { return buffer_[(head_ + index) % N]; }
  T& operator[](size_t index) { return buffer_[(head_ + index) % N]; }

 private:
  T buffer_[N > 0 ? N : 1];  // never allow a zero-sized array
  size_t head_;
  size_t count_;
};
