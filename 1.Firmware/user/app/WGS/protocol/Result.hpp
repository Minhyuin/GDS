#pragma once

#include "StmLog.hpp"

#include <cstdint>
#include <source_location>

namespace APP::WGS::PROTOCOL {

enum class ErrorCode_e : uint8_t {
  NONE = 0x00,
  CRC_ERROR = 0x01,
  BAD_LENGTH = 0x02,
  UNSUPPORTED_COMMAND = 0x03,
  INVALID_STATE = 0x04,
  OUT_OF_RANGE = 0x05,
  BUFFER_FULL = 0x06,
  BAD_OFFSET = 0x07,
  WAVE_CRC_MISMATCH = 0x08,
  DMA_BUSY = 0x09,
  UNDERRUN = 0x0A,
  TRANSPORT_BUSY = 0x0B,
};

struct Result_s {
  ErrorCode_e code{ErrorCode_e::NONE};
  bool busy_{false};

  constexpr bool ok() const {
    return code == ErrorCode_e::NONE && !busy_;
  }

  constexpr bool isBusy() const { return busy_; }

  static constexpr Result_s success() { return {}; }

  static constexpr Result_s busy() {
    return Result_s{.code = ErrorCode_e::NONE, .busy_ = true};
  }

  static Result_s
  fail(ErrorCode_e _code,
       std::source_location _loc = std::source_location::current()) {
    LOG::Logger::instance().warn(_loc, "WGS Result", "fail code: %u",
                                 static_cast<unsigned>(_code));
    return Result_s{.code = _code};
  }
};

} // namespace APP::WGS::PROTOCOL
