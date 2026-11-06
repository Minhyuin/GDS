#include "DacProxy.hpp"
#include "DacWavePlayer.hpp"
#include "OutputController.hpp"
#include "TimProxy.hpp"
extern "C" {
DAC_TypeDef dacRegisters;
DAC_HandleTypeDef hdac{&dacRegisters};
}
#include <iostream>
#include <stdexcept>
#include <string>
void require(bool b, const char *s) {
  if (!b)
    throw std::runtime_error(s);
}
struct T1 {
  static inline TIM_TypeDef reg;
  static inline TIM_HandleTypeDef h{&reg};
  static inline GPIO_TypeDef gpio;
  static GPIO_TypeDef *ackPort() { return &gpio; }
  static inline unsigned handleCalls{};
  static TIM_HandleTypeDef &handle() { ++handleCalls; return h; }
  static constexpr uint32_t ACK_PINS = GPIO_PIN_12 | GPIO_PIN_13;
  static constexpr auto ACK_CHANNEL_1 = TIM_CHANNEL_1,
                        ACK_CHANNEL_2 = TIM_CHANNEL_2;
};
struct T2 {
  static inline TIM_TypeDef reg;
  static inline TIM_HandleTypeDef h{&reg};
  static inline GPIO_TypeDef gpio;
  static GPIO_TypeDef *ackPort() { return &gpio; }
  static inline unsigned handleCalls{};
  static TIM_HandleTypeDef &handle() { ++handleCalls; return h; }
  static constexpr uint32_t ACK_PINS = GPIO_PIN_2 | GPIO_PIN_3;
  static constexpr auto ACK_CHANNEL_1 = TIM_CHANNEL_3,
                        ACK_CHANNEL_2 = TIM_CHANNEL_4;
};
using Timer1 = BSP::TimProxy<T1>;
using Timer2 = BSP::TimProxy<T2>;
struct Dac {
  static inline const uint16_t *active[2]{};
  static inline uint32_t lengths[2]{};
  static inline uint16_t expectedAtStop{};
  static inline bool inspectStop{};
  static inline uint16_t values[2]{};
  static int index(uint32_t c) { return c == DAC_CHANNEL_1 ? 0 : 1; }
  static HAL_StatusTypeDef stopDma(uint32_t c) {
    if (inspectStop && active[index(c)])
      require(*active[index(c)] == expectedAtStop,
              "DMA buffer changed before stop");
    active[index(c)] = nullptr;
    return HAL_OK;
  }
  static HAL_StatusTypeDef startDma(uint32_t c, const uint16_t *p, uint32_t n) {
    lengths[index(c)] = n;
    active[index(c)] = p;
    return HAL_OK;
  }
  static HAL_StatusTypeDef setDacValue(uint32_t c, uint32_t v) {
    values[index(c)] = v;
    return HAL_OK;
  }
  static HAL_StatusTypeDef startDac(uint32_t) { return HAL_OK; }
};
// A point ACK is a GPIO latch: timer overflow cannot lower it before the ISR.
template <typename Timer, typename Provider> void checkGpioPulseLifecycle() {
  Provider::gpio.timer = &Provider::reg;
  constexpr auto pins = Provider::ACK_PINS;
  const unsigned pin = pins == 0x3000 ? 12 : 2;
  auto mode = [&] { return ((pin < 8 ? Provider::gpio.CRL : Provider::gpio.CRH)
                           >> ((pin % 8) * 4)) & 15U; };
  Provider::gpio.ODR = pins | 1U;
  Provider::gpio.CRL = Provider::gpio.CRH = 0x44444444;
  for (float width : {1.f, 40.f, 910.2083f, 1000000.f}) {
    auto saved1 = Provider::reg.CCMR1, saved2 = Provider::reg.CCMR2;
    require(Timer::prepareSinglePulse(width) == HAL_OK, "pulse prepare failed");
    for (int repeat = 0; repeat < 2; ++repeat) {
      require(mode() == 3, "point ACK pins are not push-pull GPIO");
      const unsigned otherPin = pin + 1;
      const auto modeReg = pin < 8 ? Provider::gpio.CRL : Provider::gpio.CRH;
      require(((modeReg >> ((otherPin % 8) * 4)) & 15U) == 3,
              "second ACK pin not configured as GPIO");
      require((modeReg & 15U) == 4, "pin switch changed unrelated GPIO mode");
      const double timerTicks = (Provider::reg.ARR + 1U) * (Provider::reg.PSC + 1U);
      require(std::abs(timerTicks - width * 72.0) <= Provider::reg.PSC + 1U,
              "end timer differs by more than one prescaled tick");
      require(!(Provider::gpio.ODR & pins), "prepared ACK is high");
      require(!(Provider::reg.CR1 & TIM_CR1_CEN), "prepared timer started early");
      require((Provider::reg.CCER & 0x1111U) == 0,
              "point ACK still enables PWM outputs");
      const auto calls = Provider::handleCalls;
      require(Timer::firePreparedSinglePulseFromIsr() == HAL_OK, "pulse fire failed");
      require(Provider::handleCalls == calls, "trigger resolves timer handle");
      require((Provider::gpio.ODR & pins) == pins, "trigger did not raise ACK GPIO");
      require(Provider::gpio.riseBeforeTimer, "ACK rose after timer start");
      require(Provider::reg.CR1 & TIM_CR1_CEN, "pulse end timer not started");
      Provider::reg.CNT = 0;
      Provider::reg.CR1 &= ~TIM_CR1_CEN;
      require((Provider::gpio.ODR & pins) == pins,
              "GPIO ACK should remain high until completion IRQ");
      require(Timer::firePreparedSinglePulseFromIsr() == HAL_ERROR,
              "duplicate trigger accepted while firing");
      require(Timer::completePreparedSinglePulseFromIsr(true), "pulse completion failed");
      require(!(Provider::gpio.ODR & pins), "completion did not lower ACK");
      require(mode() == 3, "FIFO repeat left GPIO mode");
    }
    Timer::firePreparedSinglePulseFromIsr();
    Timer::cancelPreparedSinglePulse();
    require(!(Provider::gpio.ODR & pins), "cancel left ACK high");
    require((Provider::gpio.ODR & 1U) != 0, "ACK write changed unrelated GPIO");
    require(!Timer::completePreparedSinglePulseFromIsr(), "cancel left firing state");
    require(Provider::reg.CCMR1 == saved1 && Provider::reg.CCMR2 == saved2,
            "point mode did not restore timer configuration");
    require(Timer::startContinuous() == HAL_OK, "continuous restart failed");
    require(mode() == 11, "continuous ACK did not restore alternate-function mode");
    Timer::stopContinuous();
  }
  require(Timer::prepareSinglePulse(40) == HAL_OK, "pulse prepare failed");
  Timer::cancelPreparedSinglePulse();
  require(!(Provider::gpio.ODR & pins), "armed cancellation left ACK high");
  require(Timer::prepareSinglePulse(40) == HAL_OK, "pulse reprepare failed");
  Timer::firePreparedSinglePulseFromIsr();
  Provider::reg.CR1 &= ~TIM_CR1_CEN;
  Timer::completePreparedSinglePulseFromIsr(false);
  require(!(Provider::gpio.ODR & pins), "single-shot completion left ACK high");
  require(Timer::startContinuous() == HAL_OK && mode() == 11,
          "single-shot completion broke continuous mode");
  Timer::stopContinuous();
}
using namespace APP::WGS;
using namespace APP::WGS::OUTPUT;
using Player = DacWavePlayer<Dac, Timer1, Timer2>;
int main(int, char **argv) {
  try {
    std::string name = argv[1];
    if (name == "stop") {
      Timer1::startContinuous();
      Timer1::stopContinuous();
      require(!(T1::reg.CR1 & TIM_CR1_CEN),
              "continuous stop leaves CEN enabled");
    }
    if (name == "frequency") {
      for (float hz : {65535.f, 65536.f, 65537.f, 100000.f, 1.f}) {
        require(Timer1::setUpdateFrequencyHz(hz) == HAL_OK,
                "valid frequency rejected");
        double actual = 72000000.0 / (T1::reg.PSC + 1) / (T1::reg.ARR + 1);
        require(std::abs(actual - hz) / hz < .002, "wrong timer frequency");
      }
    }
    if (name == "cancel") {
      checkGpioPulseLifecycle<Timer1, T1>();
      checkGpioPulseLifecycle<Timer2, T2>();
      T1::reg.PSC = 9;
      Timer1::prepareSinglePulse(1000);
      Timer1::firePreparedSinglePulseFromIsr();
      Timer1::cancelPreparedSinglePulse();
      require(!(T1::reg.CR1 & (TIM_CR1_CEN | TIM_CR1_OPM)),
              "cancel leaves running or one-pulse timer");
      require(T1::reg.PSC == 9, "cancel failed to restore prescaler");
    }
    if (name == "busy") {
      Player p;
      WaveformStore s;
      p.start(s);
      require(p.holdChannels(1, 2, 1000).ok(), "initial point");
      p.firePendingPointPulseFromIsr();
      require(p.holdChannels(3, 4, 1000).isBusy(), "must report busy");
      require((T1::reg.DIER & TIM_DIER_UIE) && (T2::reg.DIER & TIM_DIER_UIE),
              "busy update disabled completion IRQ");
      T1::reg.CR1 &= ~TIM_CR1_CEN;
      T2::reg.CR1 &= ~TIM_CR1_CEN;
      p.completePointPulseFromIsr(OutputChannel_e::CH1);
      p.completePointPulseFromIsr(OutputChannel_e::CH2);
      require(!p.isPointPulseBusy(), "busy never cleared");
    }
    if (name == "fifo_channel") {
      Player p;
      WaveformStore s;
      p.start(s);
      p.startPointFifo(OutputChannel_e::CH1);
      PROTOCOL::PointFifoPoint_s point{{42}};
      require(p.appendPointFifo(OutputChannel_e::CH2, {&point, 1}).isBusy(),
              "enabled empty FIFO accepted different channel");
    }
    if (name == "dac_direct") {
      using HardwarePlayer = DacWavePlayer<BSP::DacProxy, Timer1, Timer2>;
      HardwarePlayer player;
      dacRegisters.CR = DAC_CR_TEN1 | (DAC_CR_TEN1 << 16);
      dacRegisters.output[0] = 1234;
      dacRegisters.output[1] = 2345;
      require(player.enterIdleLow().ok(), "idle rejected");
      require(dacRegisters.output[0] == 0 && dacRegisters.output[1] == 0,
              "idle voltage still holds previous sequence sample");
      dacRegisters.CR |= DAC_CR_TEN1;
      require(player.holdChannel(OutputChannel_e::CH1, 123, 10).ok(),
              "single point rejected");
      require(dacRegisters.output[0] == 123,
              "sequence-to-point voltage did not latch");
      player.enterIdleLow();
      dacRegisters.CR |= DAC_CR_TEN1 | (DAC_CR_TEN1 << 16);
      require(player.holdChannels(456, 789, 10).ok(), "dual point rejected");
      require(dacRegisters.output[0] == 456 && dacRegisters.output[1] == 789,
              "sequence-to-dual-point voltages did not latch");
      player.enterIdleLow();
      dacRegisters.CR = DAC_CR_TEN1 | (DAC_CR_TEN1 << 16);
      BSP::DacProxy::stopDma(DAC_CHANNEL_1);
      BSP::DacProxy::startDac(DAC_CHANNEL_1);
      BSP::DacProxy::setDacValue(DAC_CHANNEL_1, 1234);
      require(dacRegisters.output[0] == 1234,
              "point voltage still waits for timer update");
      uint16_t sample = 1;
      BSP::DacProxy::startDma(DAC_CHANNEL_1, &sample, 1);
      require(dacRegisters.CR & DAC_CR_TEN1, "sequence trigger not restored");
    }
    if (name == "sequence") {
      Player p;
      WaveformStore s;
      OutputController<Player> out(p, s);
      out.start();
      PROTOCOL::DacSample_s first{10}, next{20};
      out.dispatch(PROTOCOL::SequenceSetCommand_s{PROTOCOL::Channel_e::CH1,
                                                  {&first, 1}});
      Dac::expectedAtStop = 10;
      Dac::inspectStop = true;
      out.dispatch(
          PROTOCOL::SequenceSetCommand_s{PROTOCOL::Channel_e::CH1, {&next, 1}});
      Dac::inspectStop = false;
      require(*Dac::active[0] == 20, "new sequence not started");
    }
    if (name == "fifo_stop") {
      Player p;
      WaveformStore s;
      p.start(s);
      T1::reg.PSC = 9;
      p.startPointFifo(OutputChannel_e::CH1);
      PROTOCOL::PointFifoPoint_s point{{42}};
      p.appendPointFifo(OutputChannel_e::CH1, {&point, 1});
      p.firePendingPointPulseFromIsr();
      require(p.stopPointFifo(OutputChannel_e::CH1).ok(), "FIFO stop");
      require(!(T1::reg.CR1 & (TIM_CR1_CEN | TIM_CR1_OPM)) && T1::reg.PSC == 9,
              "FIFO stop failed to restore timer");
    }
    if (name == "mode_exclusion") {
      Player p;
      WaveformStore s;
      p.start(s);
      p.startPointFifo(OutputChannel_e::CH1);
      require(p.clearPoint2Fifo().isBusy(),
              "cross mode clear changed active FIFO");
      require(p.startPoint2Fifo().isBusy(), "cross mode start");
      p.stopPointFifo(OutputChannel_e::CH1);
      p.startPoint2Fifo();
      require(p.clearPointFifo(OutputChannel_e::CH1).isBusy(),
              "cross mode clear 2");
    }
    if (name == "active_length") {
      Player p;
      WaveformStore s;
      p.start(s);
      PROTOCOL::DacSample_s point{100};
      p.replaceSequence(OutputChannel_e::CH1, {&point, 1}, s);
      require(p.configureWaveform(OutputChannel_e::CH1, 10, 100, s).ok(),
              "config rejected");
      require(Dac::lengths[0] == 10, "running DMA length stale after config");
    }
    if (name == "invalid_input") {
      Player p;
      WaveformStore s;
      p.start(s);
      require(!p.configureWaveform(OutputChannel_e::CH1, 0, 100, s).ok(),
              "zero length accepted");
      require(!p.configureWaveform(OutputChannel_e::CH1, 1001, 100, s).ok(),
              "oversize length accepted");
      require(!p.configureWaveform(OutputChannel_e::CH1, 10, NAN, s).ok(),
              "NaN frequency accepted");
      require(!p.setPointPulseWidth(INFINITY).ok(), "infinite pulse accepted");
      require(!p.replaceSequence(OutputChannel_e::CH1, {}, s).ok(),
              "empty sequence accepted");
    }
    if (name == "gpio_fifo") {
      Player p;
      WaveformStore s;
      p.start(s);
      T1::gpio.timer = &T1::reg;
      T2::gpio.timer = &T2::reg;
      auto complete = [&] {
        T1::reg.CR1 &= ~TIM_CR1_CEN;
        T2::reg.CR1 &= ~TIM_CR1_CEN;
        p.completePointPulseFromIsr(OutputChannel_e::CH1);
        p.completePointPulseFromIsr(OutputChannel_e::CH2);
      };
      PROTOCOL::PointFifoPoint_s one{{123}};
      require(p.appendPointFifo(OutputChannel_e::CH1, {&one, 1}).ok(), "append point");
      require(p.startPointFifo(OutputChannel_e::CH1).ok(), "start point FIFO");
      T1::gpio.onRise = [] { require(Dac::values[0] == 123, "ACK preceded FIFO DAC update"); };
      p.firePendingPointPulseFromIsr();
      require((T1::gpio.ODR & T1::ACK_PINS) == T1::ACK_PINS, "FIFO ACK not high");
      require(!(T2::gpio.ODR & T2::ACK_PINS), "single FIFO fired other ACK");
      complete();
      p.firePendingPointPulseFromIsr();
      require(!(T1::gpio.ODR & T1::ACK_PINS), "empty FIFO emitted ACK");
      p.stopPointFifo(OutputChannel_e::CH1);
      T1::gpio.onRise = nullptr;
      PROTOCOL::Point2FifoPoint_s both{{456}, {789}};
      require(p.appendPoint2Fifo({&both, 1}).ok(), "append dual point");
      require(p.startPoint2Fifo().ok(), "start dual FIFO");
      T1::gpio.onRise = T2::gpio.onRise = [] {
        require(Dac::values[0] == 456 && Dac::values[1] == 789,
                "dual ACK preceded both DAC writes");
      };
      p.firePendingPointPulseFromIsr();
      require((T1::gpio.ODR & T1::ACK_PINS) && (T2::gpio.ODR & T2::ACK_PINS), "dual ACK not high");
      require(p.stopPoint2Fifo().ok(), "cancel firing FIFO");
      require(!(T1::gpio.ODR & T1::ACK_PINS) && !(T2::gpio.ODR & T2::ACK_PINS), "cancel left ACK high");
      T1::gpio.onRise = T2::gpio.onRise = nullptr;
      require(p.holdChannels(100, 200, 40).ok(), "ordinary point after FIFO");
      T2::gpio.onRise = [] {
        require(!(T1::reg.CR1 & TIM_CR1_CEN),
                "second ordinary ACK rose after first timer start");
      };
      require(!(T1::gpio.ODR & T1::ACK_PINS), "ordinary point raised ACK before EXTI");
      const auto disableCalls = irqDisableCalls;
      p.firePendingPointPulseFromIsr();
      require(irqDisableCalls == disableCalls,
              "ordinary point ISR still globally masks interrupts");
      require(p.holdChannels(300, 400, 40).isBusy(), "busy update changed result");
      require((T1::gpio.ODR & T1::ACK_PINS) && (T2::gpio.ODR & T2::ACK_PINS), "busy update truncated ACK");
      complete();
      T2::gpio.onRise = nullptr;
      require(!p.isPointPulseBusy(), "completion left point busy");
      p.firePendingPointPulseFromIsr();
      require(!(T1::gpio.ODR & T1::ACK_PINS) && !(T2::gpio.ODR & T2::ACK_PINS), "unarmed EXTI emitted ACK");
    }
    if (name == "fifo_capacity") {
      Player p;
      WaveformStore s;
      p.start(s);
      std::array<PROTOCOL::PointFifoPoint_s, 1024> points{};
      require(p.appendPointFifo(OutputChannel_e::CH1, points).ok(),
              "full FIFO rejected");
      require(
          p.appendPointFifo(OutputChannel_e::CH1, {points.data(), 1}).isBusy(),
          "FIFO overflow accepted");
      require(p.startPointFifo(OutputChannel_e::CH1).ok(), "start full FIFO");
      p.firePendingPointPulseFromIsr();
      T1::reg.CR1 &= ~TIM_CR1_CEN;
      p.completePointPulseFromIsr(OutputChannel_e::CH1);
      require(p.appendPointFifo(OutputChannel_e::CH1, {points.data(), 1}).ok(),
              "released slot cannot refill");
    }
    if (name == "repeat_start") {
      Player p;
      WaveformStore s;
      p.start(s);
      T1::reg.PSC = 9;
      p.startPointFifo(OutputChannel_e::CH1);
      require(p.startPointFifo(OutputChannel_e::CH1).ok(), "repeat start");
      p.stopPointFifo(OutputChannel_e::CH1);
      require(T1::reg.PSC == 9 && !(T1::reg.CR1 & TIM_CR1_OPM),
              "repeat start overwrote original timer config");
    }
    if (name == "other_channel") {
      Player p;
      WaveformStore s;
      p.start(s);
      PROTOCOL::DacSample_s point{100};
      p.replaceSequence(OutputChannel_e::CH2, {&point, 1}, s);
      p.startPointFifo(OutputChannel_e::CH1);
      p.stopPointFifo(OutputChannel_e::CH1);
      require(T2::reg.CR1 & TIM_CR1_CEN,
              "stopping FIFO halted unrelated sequence");
      require(T2::reg.CCER != 0, "stopping FIFO silenced unrelated ACK");
    }
    if (name == "running_frequency") {
      Timer1::startContinuous();
      require(Timer1::setUpdateFrequencyHz(20000) == HAL_OK,
              "running timer frequency change rejected by HAL busy state");
      require(T1::reg.CR1 & TIM_CR1_CEN, "frequency change halted timer");
    }
    if (name == "fifo_stop_channel") {
      Player p;
      WaveformStore s;
      p.start(s);
      p.startPointFifo(OutputChannel_e::CH1);
      require(p.stopPointFifo(OutputChannel_e::CH2).isBusy(),
              "wrong channel STOP cleared FIFO");
      require(p.clearPointFifo(OutputChannel_e::CH2).isBusy(),
              "wrong channel CLEAR cleared FIFO");
      require(p.isPointFifoActive(), "FIFO lost after wrong channel stop");
    }
    std::cout << "PASS " << name << '\n';
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "FAIL " << argv[1] << ": " << e.what() << '\n';
    return 1;
  }
}
