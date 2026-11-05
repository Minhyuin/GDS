#include "FramePolicy.hpp"
#include "ProtocolService.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <vector>

enum HAL_StatusTypeDef : int {
  HAL_OK = 0,
  HAL_ERROR = 1,
  HAL_BUSY = 2,
  HAL_TIMEOUT = 3,
};

namespace {

class ReplyTransport {
public:
  APP::WGS::PROTOCOL::Result_s tryWrite(std::span<const std::byte> _bytes) {
    writes.emplace_back(_bytes.begin(), _bytes.end());
    return APP::WGS::PROTOCOL::Result_s::success();
  }

  std::vector<std::vector<std::byte>> writes;
};

using Policy = APP::WGS::PROTOCOL::GdsFrameV1Policy_s;
using Service = APP::WGS::COMM::ProtocolService<ReplyTransport, Policy>;

bool checkAck() {
  ReplyTransport transport;
  Service service(transport);
  const auto result =
      service.sendAck(APP::WGS::PROTOCOL::AckPayload_s{
          .status = APP::WGS::PROTOCOL::AckStatus_e::DONE,
          .detail = APP::WGS::PROTOCOL::ErrorCode_e::NONE,
      });
  if (!result.ok() || transport.writes.size() != 1 ||
      transport.writes[0].size() != 6) {
    std::cerr << "FAIL: ACK must be a single six-byte reply\n";
    return false;
  }
  const std::vector<std::byte> expected{
      std::byte{0xA5}, std::byte{0x02}, std::byte{0x02}, std::byte{0x00},
      std::byte{0x01}, std::byte{0x00}};
  if (transport.writes[0] != expected) {
    std::cerr << "FAIL: ACK bytes differ from GDS v1\n";
    return false;
  }
  return true;
}

bool checkNack() {
  ReplyTransport transport;
  Service service(transport);
  const auto result = service.sendNack(
      APP::WGS::PROTOCOL::ErrorCode_e::OUT_OF_RANGE);
  if (!result.ok() || transport.writes.size() != 1 ||
      transport.writes[0].size() != 6) {
    std::cerr << "FAIL: NACK must be a single six-byte reply\n";
    return false;
  }
  return true;
}

} // namespace

int main() {
  const bool ok = checkAck() && checkNack();
  std::cout << (ok ? "RESULT PASS\n" : "RESULT FAIL\n");
  return ok ? 0 : 1;
}
