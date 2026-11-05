#include <cstddef>
#include <cstdint>

enum HAL_StatusTypeDef : int {
  HAL_OK = 0,
  HAL_ERROR = 1,
  HAL_BUSY = 2,
  HAL_TIMEOUT = 3,
};

#define __HAL_DMA_DISABLE_IT(handle, interrupt) ((void)(handle), (void)(interrupt))

#include "UsartDmaTransport.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <span>
#include <string_view>
#include <vector>

namespace {

using APP::WGS::COMM::TransportState_e;
using APP::WGS::COMM::UsartDmaTransport;

class FakeProxy {
public:
  static HAL_StatusTypeDef startRxDma(uint8_t *_buffer, size_t _size) {
    rxBuffer = _buffer;
    rxSize = _size;
    ++startCount;
    if (startStatus != HAL_OK) {
      return startStatus;
    }
    dmaRemaining = _size;
    return HAL_OK;
  }

  static HAL_StatusTypeDef transmitDma(uint8_t *_buffer, size_t _size) {
    ++asyncWriteCount;
    lastAsyncBuffer = _buffer;
    lastAsyncSize = _size;
    return HAL_OK;
  }

  static HAL_StatusTypeDef transmitBlocking(uint8_t *_buffer, size_t _size,
                                            uint32_t _timeoutMs) {
    if (blockingStatus != HAL_OK) {
      return blockingStatus;
    }
    syncWrites.emplace_back(_buffer, _buffer + _size);
    syncTimeouts.push_back(_timeoutMs);
    return HAL_OK;
  }

  static uint32_t irqSave() {
    const uint32_t primask = irqMask;
    irqMask = 1;
    return primask;
  }

  static void irqRestore(uint32_t _primask) { irqMask = _primask; }

  static void stopDma() { ++stopCount; }

  static size_t rxDmaRemaining() { return dmaRemaining; }

  static void reset() {
    rxBuffer = nullptr;
    rxSize = 0;
    dmaRemaining = 0;
    asyncWriteCount = 0;
    lastAsyncBuffer = nullptr;
    lastAsyncSize = 0;
    syncWrites.clear();
    syncTimeouts.clear();
    startCount = 0;
    stopCount = 0;
    startStatus = HAL_OK;
    blockingStatus = HAL_OK;
    irqMask = 0;
  }

  static inline uint8_t *rxBuffer{nullptr};
  static inline size_t rxSize{0};
  static inline size_t dmaRemaining{0};
  static inline size_t asyncWriteCount{0};
  static inline uint8_t *lastAsyncBuffer{nullptr};
  static inline size_t lastAsyncSize{0};
  static inline std::vector<std::vector<uint8_t>> syncWrites{};
  static inline std::vector<uint32_t> syncTimeouts{};
  static inline size_t startCount{0};
  static inline size_t stopCount{0};
  static inline HAL_StatusTypeDef startStatus{HAL_OK};
  static inline HAL_StatusTypeDef blockingStatus{HAL_OK};
  static inline uint32_t irqMask{0};
};

using Transport = UsartDmaTransport<FakeProxy>;

bool gFailed = false;

void require(bool _condition, std::string_view _message) {
  if (!_condition) {
    std::cerr << "FAIL: " << _message << '\n';
    gFailed = true;
  }
}

void fillRx(size_t _offset, size_t _count, uint8_t _seed = 0) {
  require(FakeProxy::rxBuffer != nullptr, "RX DMA buffer was not registered");
  if (FakeProxy::rxBuffer == nullptr) {
    return;
  }
  for (size_t i = 0; i < _count; ++i) {
    FakeProxy::rxBuffer[(_offset + i) % Transport::RX_BUFFER_SIZE] =
        static_cast<uint8_t>(_seed + i);
  }
}

template <typename T> void publishRxEvent(T &_transport, size_t _position) {
  FakeProxy::dmaRemaining =
      Transport::RX_BUFFER_SIZE - (_position % Transport::RX_BUFFER_SIZE);
  if constexpr (requires { _transport.onRxEventFromIsr(uint16_t{}); }) {
    _transport.onRxEventFromIsr(static_cast<uint16_t>(_position));
  }
}

template <typename Span>
concept HasOverwriteFlag = requires(const Span &_span) { _span.overwritten; };

template <typename Span, typename TransportT>
bool replacedByOverwriteFlag(const Span &_span, const TransportT &) {
  if constexpr (HasOverwriteFlag<Span>) {
    return _span.overwritten;
  }
  return false;
}

std::vector<uint8_t> consumeAll(Transport &_transport,
                                bool &_overwritten) {
  std::vector<uint8_t> bytes;
  _overwritten = false;
  for (size_t i = 0; i < 128; ++i) {
    const auto span = _transport.consumeRx();
    if (span.bytes.empty()) {
      break;
    }
    require(span.bytes.size() <= 256,
            "transport must hand parser a short scratch copy, not a DMA span");
    for (const auto byte : span.bytes) {
      bytes.push_back(std::to_integer<uint8_t>(byte));
    }
    _overwritten |= replacedByOverwriteFlag(span, _transport);
  }
  return bytes;
}

void testFullCircleEventIsNotLost() {
  FakeProxy::reset();
  Transport transport;
  require(transport.start().ok(), "transport start should succeed");

  fillRx(0, Transport::RX_BUFFER_SIZE, 0x5A);
  publishRxEvent(transport, Transport::RX_BUFFER_SIZE);

  bool overwritten = false;
  const auto bytes = consumeAll(transport, overwritten);
  require(bytes.size() == Transport::RX_BUFFER_SIZE,
          "one full DMA revolution must expose all bytes, not zero");
  require(!overwritten, "full revolution without consumer lag is not overwrite");
}

void testContinuousStreamAcrossWrap() {
  FakeProxy::reset();
  Transport transport;
  require(transport.start().ok(), "transport start should succeed");

  fillRx(0, Transport::RX_BUFFER_SIZE / 2, 0x11);
  publishRxEvent(transport, Transport::RX_BUFFER_SIZE / 2);

  bool overwritten = false;
  auto bytes = consumeAll(transport, overwritten);
  require(bytes.size() == Transport::RX_BUFFER_SIZE / 2,
          "half-transfer event should expose first half");

  fillRx(Transport::RX_BUFFER_SIZE / 2, Transport::RX_BUFFER_SIZE / 2, 0x22);
  publishRxEvent(transport, Transport::RX_BUFFER_SIZE);
  bytes = consumeAll(transport, overwritten);
  require(bytes.size() == Transport::RX_BUFFER_SIZE / 2,
          "transfer-complete event should expose second half");

  fillRx(0, 1500, 0x33);
  publishRxEvent(transport, 1500);
  bytes = consumeAll(transport, overwritten);
  require(bytes.size() == 1500,
          "post-wrap idle event should expose bytes at the start of the ring");
  require(bytes.front() == 0x33, "post-wrap byte order should start at offset 0");
}

void testOverwriteDropsBacklogAndRequestsRestart() {
  FakeProxy::reset();
  Transport transport;
  require(transport.start().ok(), "transport start should succeed");

  fillRx(0, Transport::RX_BUFFER_SIZE / 2, 0x44);
  publishRxEvent(transport, Transport::RX_BUFFER_SIZE / 2);
  fillRx(Transport::RX_BUFFER_SIZE / 2, Transport::RX_BUFFER_SIZE / 2, 0x66);
  publishRxEvent(transport, Transport::RX_BUFFER_SIZE);

  fillRx(0, 1000, 0x77);
  publishRxEvent(transport, 1000);

  const auto rx = transport.consumeRx();
  require(rx.bytes.empty(),
          "lapped-ring backlog must be discarded rather than parsed");
  require(rx.overwritten, "consumer lag beyond the RX ring must be reported");
  require(transport.hasPendingError(),
          "overwrite must request task-side parser and RX recovery");

  FakeProxy::startStatus = HAL_OK;
  require(transport.restart().ok(),
          "overwrite recovery should succeed when HAL restarts RX DMA");
  require(!transport.hasPendingError(),
          "successful restart should clear the pending RX error");
}

void testNdtrCapturesInFlightBytesBeforeCallback() {
  FakeProxy::reset();
  Transport transport;
  require(transport.start().ok(), "transport start should succeed");

  fillRx(0, 300, 0x0A);
  // Simulate DMA progress between HT events: no callback has advanced the
  // event baseline yet, but NDTR shows 300 bytes have already been written.
  FakeProxy::dmaRemaining = Transport::RX_BUFFER_SIZE - 300;

  bool overwritten = false;
  const auto bytes = consumeAll(transport, overwritten);
  require(bytes.size() == 300,
          "NDTR snapshot should expose bytes received before the next callback");
  require(!overwritten, "small in-flight backlog must not be treated as overwrite");
}

void testLateIrqServicePastHalfRingRequestsRestart() {
  FakeProxy::reset();
  Transport transport;
  require(transport.start().ok(), "transport start should succeed");

  // No HT/TC/IDLE callback has advanced the event baseline and DMA is already
  // more than one half-ring ahead. Position modulo the ring cannot distinguish
  // this from missed callbacks, so the hard service deadline must fail closed.
  FakeProxy::dmaRemaining =
      Transport::RX_BUFFER_SIZE - (Transport::RX_BUFFER_SIZE / 2 + 1);
  const auto rx = transport.consumeRx();
  require(rx.bytes.empty(), "late IRQ service must not expose ambiguous bytes");
  require(rx.overwritten,
          "late IRQ service beyond half a ring must request RX restart");
  require(transport.hasPendingError(),
          "late IRQ service must set the pending recovery error");
}

void testDuplicateIdleAtHalfAndFullPositionsDoesNotRepeatData() {
  FakeProxy::reset();
  Transport transport;
  require(transport.start().ok(), "transport start should succeed");

  fillRx(0, Transport::RX_BUFFER_SIZE / 2, 0x21);
  publishRxEvent(transport, Transport::RX_BUFFER_SIZE / 2);
  bool overwritten = false;
  auto bytes = consumeAll(transport, overwritten);
  require(bytes.size() == Transport::RX_BUFFER_SIZE / 2,
          "half-transfer callback should expose the first half once");

  publishRxEvent(transport, Transport::RX_BUFFER_SIZE / 2);
  bytes = consumeAll(transport, overwritten);
  require(bytes.empty(),
          "idle callback repeating the half position must not repeat bytes");

  fillRx(Transport::RX_BUFFER_SIZE / 2, Transport::RX_BUFFER_SIZE / 2, 0x31);
  publishRxEvent(transport, Transport::RX_BUFFER_SIZE);
  bytes = consumeAll(transport, overwritten);
  require(bytes.size() == Transport::RX_BUFFER_SIZE / 2,
          "transfer-complete callback should expose the second half once");

  publishRxEvent(transport, Transport::RX_BUFFER_SIZE);
  bytes = consumeAll(transport, overwritten);
  require(bytes.empty(),
          "idle callback repeating the full-ring position must not repeat bytes");
  // Repeated 8K callbacks are only disambiguated when HT is delivered between
  // revolutions. UsartProxy keeps HAL HT enabled; receive service latency must
  // therefore stay below one half-ring fill interval (about 44 ms at 921600).
}

void testFullRingBoundaryThenNextHalfStartsAtZero() {
  FakeProxy::reset();
  Transport transport;
  require(transport.start().ok(), "transport start should succeed");

  fillRx(0, Transport::RX_BUFFER_SIZE, 0x40);
  publishRxEvent(transport, Transport::RX_BUFFER_SIZE);
  bool overwritten = false;
  auto bytes = consumeAll(transport, overwritten);
  require(bytes.size() == Transport::RX_BUFFER_SIZE,
          "full-ring boundary should expose exactly one ring of bytes");

  fillRx(0, Transport::RX_BUFFER_SIZE / 2, 0x71);
  publishRxEvent(transport, Transport::RX_BUFFER_SIZE / 2);
  bytes = consumeAll(transport, overwritten);
  require(bytes.size() == Transport::RX_BUFFER_SIZE / 2,
          "next half callback after wrap should expose the new half");
  require(!bytes.empty() && bytes.front() == 0x71,
          "post-wrap production must restart at ring offset zero");
}

void testScratchCopyIsStableAfterDmaRingChanges() {
  FakeProxy::reset();
  Transport transport;
  require(transport.start().ok(), "transport start should succeed");

  fillRx(0, 1000, 0x19);
  publishRxEvent(transport, 1000);
  const auto rx = transport.consumeRx();
  require(!rx.bytes.empty(), "scratch copy should expose received bytes");
  require(reinterpret_cast<const uint8_t *>(rx.bytes.data()) !=
              FakeProxy::rxBuffer,
          "consumer span must not alias the DMA-owned ring");

  std::vector<uint8_t> copied;
  copied.reserve(rx.bytes.size());
  for (const auto byte : rx.bytes) {
    copied.push_back(std::to_integer<uint8_t>(byte));
  }

  fillRx(0, 1000, 0xEE);
  bool stable = copied.size() == rx.bytes.size();
  for (size_t i = 0; stable && i < copied.size(); ++i) {
    stable = copied[i] == std::to_integer<uint8_t>(rx.bytes[i]);
  }
  require(stable,
          "scratch copy must remain stable when DMA later rewrites the ring");
}

void testSynchronousTxCopiesEachShortReply() {
  FakeProxy::reset();
  Transport transport;
  require(transport.start().ok(), "transport start should succeed");

  const std::array<std::byte, 6> first{
      std::byte{0xA5}, std::byte{0x02}, std::byte{0x02}, std::byte{0x00},
      std::byte{0x01}, std::byte{0x00}};
  const std::array<std::byte, 6> second{
      std::byte{0xA5}, std::byte{0x03}, std::byte{0x02}, std::byte{0x00},
      std::byte{0x80}, std::byte{0x04}};

  require(transport.tryWrite(first).ok(), "first short reply should be written");
  require(transport.tryWrite(second).ok(),
          "second short reply should not observe a fake TX-busy state");
  require(FakeProxy::syncWrites.size() == 2,
          "each reply must be copied by a task-side synchronous transmit");
  require(FakeProxy::asyncWriteCount == 0,
          "reply TX must not retain a pointer for asynchronous DMA");
  if (FakeProxy::syncWrites.size() == 2) {
    const std::vector<uint8_t> firstBytes(
        reinterpret_cast<const uint8_t *>(first.data()),
        reinterpret_cast<const uint8_t *>(first.data()) + first.size());
    const std::vector<uint8_t> secondBytes(
        reinterpret_cast<const uint8_t *>(second.data()),
        reinterpret_cast<const uint8_t *>(second.data()) + second.size());
    require(FakeProxy::syncWrites[0] == firstBytes,
            "first reply bytes must stay intact after the second reply");
    require(FakeProxy::syncWrites[1] == secondBytes,
            "second reply bytes should be copied in call order");
  }
}

template <typename TransportT>
void testErrorRestartIsCheckedInTaskContextApi() {
  FakeProxy::reset();
  TransportT transport;
  require(transport.start().ok(), "transport start should succeed");

  if constexpr (requires(TransportT &_t) {
                  _t.onErrorFromIsr();
                  _t.restart();
                }) {
    transport.onErrorFromIsr();
    FakeProxy::startStatus = HAL_BUSY;
    const auto failed = transport.restart();
    require(!failed.ok(), "restart must report a HAL start failure");
    require(transport.state() == TransportState_e::FAULT,
            "failed RX restart should leave transport in FAULT");

    FakeProxy::startStatus = HAL_OK;
    require(transport.hasPendingError(),
            "failed restart must remain pending for explicit retry");
    const auto recovered = transport.restart();
    require(recovered.ok(), "restart should recover when HAL accepts RX DMA");
    require(transport.state() == TransportState_e::READY,
            "successful RX restart should return transport to READY");
    require(!transport.hasPendingError(),
            "successful restart must clear the pending RX error");
  } else {
    require(false, "transport needs task-side onErrorFromIsr/restart API");
  }
}

} // namespace

int main() {
  testFullCircleEventIsNotLost();
  testContinuousStreamAcrossWrap();
  testOverwriteDropsBacklogAndRequestsRestart();
  testNdtrCapturesInFlightBytesBeforeCallback();
  testLateIrqServicePastHalfRingRequestsRestart();
  testDuplicateIdleAtHalfAndFullPositionsDoesNotRepeatData();
  testFullRingBoundaryThenNextHalfStartsAtZero();
  testScratchCopyIsStableAfterDmaRingChanges();
  testSynchronousTxCopiesEachShortReply();
  testErrorRestartIsCheckedInTaskContextApi<Transport>();
  if (gFailed) {
    std::cerr << "RESULT FAIL\n";
    return 1;
  }
  std::cout << "RESULT PASS\n";
  return 0;
}
