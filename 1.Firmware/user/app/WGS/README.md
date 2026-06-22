# WGS - Waveform Generation System

WGS 是固件中的波形生成系统模块，负责接收上位机命令、解析协议帧、控制 DAC 周期波形表，并最终通过 DAC 输出控制电压。

当前 WGS 运行在 FreeRTOS 上。USART1 receive-to-idle 回调只负责通知 WGS 任务，实际的数据消费、协议解析、命令处理、DAC 周期表更新和应答发送都在 WGS worker task 中完成。DAC 输出由硬件定时器触发：

```text
CH1 / DAC_OUT1 <- TIM4 TRGO
CH2 / DAC_OUT2 <- TIM5 TRGO
```

每个通道拥有 1000 个周期样本。DAC circular DMA 扫描这 1000 个样本，因此 DMA 区域本身就是一个波形周期。

## 模块边界

```text
PC / MATLAB / Host
  |
  | UART binary frame
  v
BSP::UsartProxy<...>
  |
  v
APP::WGS::COMM::UsartDmaTransport
  |
  v
APP::WGS::PROTOCOL::ProtocolParser
  |
  v
APP::WGS::WGSManager
  |
  +--> APP::WGS::COMM::ProtocolService   -> ACK / NACK
  |
  +--> APP::WGS::OUTPUT::OutputController
          |
          v
        APP::WGS::OUTPUT::DacWavePlayer
          |
          v
        BSP::DacProxy + BSP::TimProxy<...>
```

WGS 不直接持有 HAL 句柄。底层外设访问通过 BSP proxy 注入：

```text
BSP::UsartProxy<BSP::Usart1HandleProvider>
BSP::DacProxy
BSP::TimProxy<BSP::Tim4HandleProvider>
BSP::TimProxy<BSP::Tim5HandleProvider>
```

## RTOS 行为

WGS 初始化时会创建一个静态 FreeRTOS 任务：

```cpp
xTaskCreateStatic(..., "TASK_NAME", ...);
```

任务主循环阻塞等待通知：

```cpp
ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
```

没有接收事件时，WGS 任务不会轮询占用 CPU。USART1 receive-to-idle 回调调用 `notifyWgsRxFromIsr()` 唤醒任务。真正的 RX buffer 消费、协议解析和应答发送都在任务上下文中执行。

## 协议帧格式

当前协议使用小端字段，帧头尽量压缩以降低串口开销。

```text
offset  size  field
0       1     SOF = 0xA5
1       1     frame type
2       2     payload length, uint16 little-endian
4       N     payload
```

当前版本暂不使用 CRC，也不携带 version 和 seq。

当前最大 payload 长度：

```text
4004 bytes
```

这正好可以容纳一个通道 1000 个 float32 电压样本：

```text
command(1) + channel(1) + count(2) + 1000 * float32(4) = 4004
```

## Frame Type

```cpp
COMMAND = 0x01
ACK     = 0x02
NACK    = 0x03
EVENT   = 0x04
STATUS  = 0x05
```

上位机发送命令时，`frame type` 应为 `0x01 COMMAND`。

## Channel

```cpp
CH1 = 0x01  // DAC_OUT1, TIM4
CH2 = 0x02  // DAC_OUT2, TIM5
```

## Command Payload

命令帧 payload 的第一个字节是命令类型：

```text
payload[0] = command type
```

当前已实现：

```cpp
PING         = 0x00
POINT_SET    = 0x10
SEQUENCE_SET = 0x20
WAVEFORM_CONFIG = 0x30
TIMER_SET    = 0x40
```

## PING

用途：检查通信链路和协议解析是否正常。

```text
offset  size  field
0       1     command type = 0x00
```

示例：

```text
A5 01 01 00 00
```

成功时固件返回 `ACK / DONE`。

## POINT_SET

用途：使某个通道持续输出目标电压。固件会停止该通道 DAC DMA，直接写入 DAC value，并确保对应 timer 运行，以便外部触发配置能把 DHR 刷新到实际输出寄存器。该模式不会启动 DMA 扫描。

```text
offset  size  field
0       1     command type = 0x10
1       1     channel
2       4     voltage, float32 little-endian, unit = V
6       4     ack_pulse_us, float32 little-endian, unit = us
```

电压范围：

```text
0.0..3.3 V
```

固件会将电压线性映射到 12 位 DAC code：

```text
dac_code = round(voltage_v * 4095 / 3.3)
```

若电压为 `NaN`、`Inf`、负数或超过 `3.3V`，固件会钳位到 `0.0..3.3V` 范围内，不返回范围错误。

ACK 脉宽范围：

```text
1..1000000 us
```

若 ACK 脉宽为 `NaN`、`Inf`、小于 `1us` 或大于 `1s`，固件返回 `NACK / OUT_OF_RANGE`。

示例：设置 CH1 = 1.5V，ACK 脉宽 5us：

```text
A5 01 0A 00 10 01 00 00 C0 3F 00 00 A0 40
```

## SEQUENCE_SET

用途：写入某个通道的一周期波形。每帧只操作一个通道。上位机推荐先发送 `WAVEFORM_CONFIG` 设置当前通道的波形长度和频率，再发送 `SEQUENCE_SET`。

```text
offset  size      field
0       1         command type = 0x20
1       1         channel
2       2         sample count N, uint16 little-endian
4       N * 4     voltage samples, float32 little-endian, unit = V
```

约束：

```text
1 <= N <= 1000
payload length = 4 + N * 4
voltage clamp range = 0.0..3.3V
```

如果 `N` 超过当前通道的波形长度，固件会截断到当前波形长度；如果 `N` 小于当前通道的波形长度，固件用最后一个样本填满剩余位置：

```text
periodBuffer[N..active_length - 1] = periodBuffer[N - 1]
```

如果 `N = 0`、`N > 1000` 或 payload 长度不匹配，固件返回 `NACK / BAD_LENGTH`。

示例：向 CH2 写入两个样本 `1.65V, 3.30V`，剩余 998 个点保持 `3.30V`：

```text
payload:
20 02 02 00
33 33 D3 3F
33 33 53 40
```

## WAVEFORM_CONFIG

用途：设置某个通道的当前波形长度和波形重复频率。固件会计算采样率：

```text
sample_rate_hz = length_sa * frequency_hz
```

并配置对应通道的 TIM4/TIM5 触发频率。

```text
offset  size  field
0       1     command type = 0x30
1       1     channel
2       2     length_sa, uint16 little-endian, unit = Sa
4       4     frequency_hz, float32 little-endian, unit = Hz
```

约束：

```text
1 <= length_sa <= 1000
frequency_hz > 0
length_sa * frequency_hz <= 100000 Sa/s
```

长度非法时返回 `NACK / BAD_LENGTH`；频率或采样率越界时返回 `NACK / OUT_OF_RANGE`。

## TIMER_SET

用途：直接修改某个通道的 DAC 采样率。CH1 对应 TIM4，CH2 对应 TIM5。一般上位机应优先使用 `WAVEFORM_CONFIG`，由波形长度和频率推导采样率。

```text
offset  size  field
0       1     command type = 0x40
1       1     channel
2       4     sample_rate_hz, float32 little-endian
```

限制：

```text
1 Hz <= sample_rate_hz <= 100000 Hz
```

频率越界时返回 `NACK / OUT_OF_RANGE`，不会钳位。

## ACK / NACK Payload

应答 payload 固定 2 字节：

```text
offset  size  field
0       1     status
1       1     detail
```

状态码：

```cpp
ACCEPTED = 0x00
DONE     = 0x01
BUSY     = 0x02
ERROR    = 0x80
```

错误码：

```cpp
NONE                = 0x00
CRC_ERROR           = 0x01
BAD_LENGTH          = 0x02
UNSUPPORTED_COMMAND = 0x03
INVALID_STATE       = 0x04
OUT_OF_RANGE        = 0x05
BUFFER_FULL         = 0x06
BAD_OFFSET          = 0x07
WAVE_CRC_MISMATCH   = 0x08
DMA_BUSY            = 0x09
UNDERRUN            = 0x0A
TRANSPORT_BUSY      = 0x0B
```

成功应答示例：

```text
A5 02 02 00 01 00
```

含义：

```text
ACK, payload length = 2, status = DONE, detail = NONE
```

## Host 端建议

上位机每发送一个命令帧后，应等待 ACK/NACK，再发送下一帧。推荐使用 `test/wgs_matlab` 中的 MATLAB client：

```matlab
client = wgs.connect("COM43");
cleanupObj = onCleanup(@() client.close());

client.sequenceSet(1, ch1Samples, 100);  % config length/frequency first
client.sequenceSet(2, ch2Samples, 100);
```
