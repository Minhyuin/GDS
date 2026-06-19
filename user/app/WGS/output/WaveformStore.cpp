#include "WaveformStore.hpp"

namespace APP::WGS::OUTPUT {

APP::WGS::PROTOCOL::Result_s
WaveformStore::push(std::span<const DacFrame_s> _frames) {
  if (_frames.empty()) {
    return APP::WGS::PROTOCOL::Result_s::fail(
        APP::WGS::PROTOCOL::ErrorCode_e::BAD_LENGTH);
  }

  if (_frames.size() > writable()) {
    return APP::WGS::PROTOCOL::Result_s::fail(
        APP::WGS::PROTOCOL::ErrorCode_e::BUFFER_FULL);
  }

  for (const auto &frame : _frames) {
    frames_[head_] = frame;
    head_ = next(head_);
  }

  return APP::WGS::PROTOCOL::Result_s::success();
}

bool WaveformStore::popFromIsr(DacFrame_s &_frame) {
  if (head_ == tail_) {
    return false;
  }

  _frame = frames_[tail_];
  tail_ = next(tail_);
  return true;
}

void WaveformStore::clear() {
  tail_ = head_;
}

size_t WaveformStore::size() const {
  const size_t head = head_;
  const size_t tail = tail_;
  if (head >= tail) {
    return head - tail;
  }
  return CAPACITY - tail + head;
}

size_t WaveformStore::writable() const {
  return CAPACITY - size() - 1U;
}

} // namespace APP::WGS::OUTPUT
