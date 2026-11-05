#pragma once

#include <cstddef>
#include <span>

namespace APP::WGS::COMM {

struct RxSpan_s {
  std::span<const std::byte> bytes{};
  bool overwritten{false};
};

} // namespace APP::WGS::COMM
