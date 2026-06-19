#pragma once

#include "DacTypes.hpp"
#include "Result.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace APP::WGS::OUTPUT {

class WaveformStore {
public:
  static constexpr size_t CAPACITY = 512;

  APP::WGS::PROTOCOL::Result_s push(std::span<const DacFrame_s> _frames);
  bool popFromIsr(DacFrame_s &_frame);
  void clear();
  size_t size() const;
  size_t writable() const;

private:
  size_t next(size_t _index) const { return (_index + 1U) % CAPACITY; }

  DacFrame_s frames_[CAPACITY]{};
  volatile size_t head_{0};
  volatile size_t tail_{0};
};

} // namespace APP::WGS::OUTPUT
