#pragma once

#include "Result.hpp"

#include <cstdint>

namespace APP::WGS::PROTOCOL {

enum class AckStatus_e : uint8_t {
  ACCEPTED = 0x00,
  DONE = 0x01,
  BUSY = 0x02,
  ERROR = 0x80,
};

struct AckPayload_s {
  AckStatus_e status{AckStatus_e::DONE};
  ErrorCode_e detail{ErrorCode_e::NONE};
};

} // namespace APP::WGS::PROTOCOL
