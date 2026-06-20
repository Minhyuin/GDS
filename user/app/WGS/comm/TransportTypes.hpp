#pragma once

#include <cstddef>
#include <span>

namespace APP::WGS::COMM {

struct RxSpan_s {
  std::span<const std::byte> bytes{};
};

} // namespace APP::WGS::COMM
