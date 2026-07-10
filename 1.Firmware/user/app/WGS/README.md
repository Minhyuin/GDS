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
SYNC         = 0x01
POINT_SET    = 0x10
POINT2_SET   = 0x11
POINT_SET_NO_ACK  = 0x12
POINT2_SET_NO_ACK = 0x13
POINT_FIFO_CLEAR   = 0x14
POINT_FIFO_APPEND  = 0x15
POINT_FIFO_START   = 0x16
POINT_FIFO_STOP    = 0x17
POINT2_FIFO_CLEAR  = 0x20
POINT2_FIFO_APPEND = 0x21
POINT2_FIFO_START  = 0x22
POINT2_FIFO_STOP   = 0x23
WAVEFORM_CONFIG = 0x30
TIMER_SET    = 0x40
POINT_PULSE_WIDTH_SET = 0x50
SEQUENCE_SET = 0x60
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

用途：设置某个通道的单点目标电压，并等待外部触发输入 `EXT_PULSE_IN` 的上升沿到达后，再输出一次单点脉冲。固件会停止该通道 DAC DMA，直接写入 DAC value，并预配置对应 timer 的 one-pulse 输出；命令本身不会立即输出脉冲。

```text
offset  size  field
0       1     command type = 0x10
1       1     channel
2       4     voltage, float32 little-endian, unit = V
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

成功时固件返回 `ACK / DONE / NONE`，含义是电压已经设置并且单点脉冲已经 armed。下一次 `EXT_PULSE_IN` 上升沿到达时，固件才会输出脉冲。外部脉冲到达时如果没有 pending 的单点命令，固件会忽略该触发。

如果已有 pending 或 firing 的单点脉冲，新的 `POINT_SET` / `POINT2_SET` 仍会更新 DAC
目标电压，但不会重新 armed 单点脉冲，并返回：

```text
ACK / BUSY / NONE
```

`POINT_PULSE_WIDTH_SET`、`SEQUENCE_SET`、`SYNC`、`WAVEFORM_CONFIG` 和 `TIMER_SET` 在
pending 或 firing 期间仍直接返回 `ACK / BUSY / NONE`，不会修改对应配置。`PING` 不受影响。

`POINT_SET_NO_ACK = 0x12` 使用同样的 payload 格式：

```text
offset  size  field
0       1     command type = 0x12
1       1     channel
2       4     voltage, float32 little-endian, unit = V
```

有效的 `POINT_SET_NO_ACK` 会执行与 `POINT_SET` 相同的 DAC 更新和单点 armed 逻辑，但不会返回
`ACK / DONE`、`ACK / BUSY` 或运行期 `NACK`。帧格式错误、长度错误或不支持的命令仍可能返回 NACK。

示例：设置 CH1 = 1.5V，并等待下一次外部上升沿触发输出：

```text
A5 01 06 00 10 01 00 00 C0 3F
```

## POINT2_SET

用途：在一帧内同时设置 CH1 和 CH2 的单点目标电压，并等待同一个 `EXT_PULSE_IN` 上升沿触发两路单点脉冲。两路 timer 在同一个 EXTI ISR 内先后启动，因此两路脉冲响应同一个外部触发，但不声明硬件零相位同步。

```text
offset  size  field
0       1     command type = 0x11
1       4     ch1 voltage, float32 little-endian, unit = V
5       4     ch2 voltage, float32 little-endian, unit = V
```

成功时固件返回 `ACK / DONE / NONE`。如果已有 pending 或 firing 的单点脉冲，固件仍会更新
CH1 / CH2 的 DAC 目标电压，但不会重新 armed 单点脉冲，并返回 `ACK / BUSY / NONE`。

`POINT2_SET_NO_ACK = 0x13` 使用同样的双通道 payload 格式：

```text
offset  size  field
0       1     command type = 0x13
1       4     ch1 voltage, float32 little-endian, unit = V
5       4     ch2 voltage, float32 little-endian, unit = V
```

有效的 `POINT2_SET_NO_ACK` 会执行与 `POINT2_SET` 相同的两路 DAC 更新和单点 armed 逻辑，但不会返回
`ACK / DONE`、`ACK / BUSY` 或运行期 `NACK`。

## POINT FIFO

用途：把单通道单点轨迹预先缓存到固件 FIFO 中，再由 `EXT_PULSE_IN` 的每个上升沿消费一个点。该模式适合只扫描 CH1 或 CH2 的外部触发单点输出。

固件 FIFO 深度为 `1024` 个单通道点。每个点包含一个 12 位 DAC code。上位机追加轨迹时仍发送电压，固件在解码阶段转换为 DAC code。当前实现一次只允许一个单通道 POINT FIFO 活跃；如果 FIFO 中已有 CH1 点，再向 CH2 追加会返回 `ACK / BUSY / NONE`。

### POINT_FIFO_CLEAR

```text
offset  size  field
0       1     command type = 0x14
1       1     channel
```

清空单通道 FIFO，停止 POINT FIFO 外部触发输出，并取消已准备的单点脉冲。成功返回 `ACK / DONE / NONE`。

### POINT_FIFO_APPEND

```text
offset  size  field
0       1     command type = 0x15
1       1     channel
2       2     point_count, uint16 little-endian
4       4*N   N * voltage float32_le
```

单帧最多追加 `1000` 个点，这是由当前协议最大 payload `4004 bytes` 决定的。若 FIFO 剩余空间不足以完整写入这一帧，固件返回 `ACK / BUSY / NONE`，不会写入这一批点；上位机应稍后重试。

### POINT_FIFO_START

```text
offset  size  field
0       1     command type = 0x16
1       1     channel
```

开启对应通道的 FIFO 外部触发输出。开启后每个 `EXT_PULSE_IN` 上升沿：

```text
如果 FIFO 非空：pop 一个点 -> 更新对应 DAC -> 启动对应 TIM one-pulse
如果 FIFO 为空：忽略该外部触发
```

### POINT_FIFO_STOP

```text
offset  size  field
0       1     command type = 0x17
1       1     channel
```

停止 POINT FIFO 外部触发输出并清空 FIFO。这样 stop 后不会残留旧轨迹。

POINT FIFO 和 POINT2 FIFO 互斥。任一 FIFO active 期间，普通 `POINT_SET`、`POINT2_SET`、`POINT_PULSE_WIDTH_SET`、`SEQUENCE_SET`、`SYNC`、`WAVEFORM_CONFIG` 和 `TIMER_SET` 返回 `ACK / BUSY / NONE`。如需切换模式，先发送对应 FIFO 的 `STOP` 或 `CLEAR`。

## POINT2 FIFO

用途：把双通道单点轨迹预先缓存到固件 FIFO 中，再由 `EXT_PULSE_IN` 的每个上升沿消费一个点。该模式用于外部触发频率接近或高于上位机逐点命令频率时，避免 `POINT2_SET_NO_ACK` 受 PC 命令到达相位影响。

固件 FIFO 深度为 `1024` 个双通道点。每个点包含 CH1 / CH2 两个 12 位 DAC code。上位机追加轨迹时仍发送电压，固件在解码阶段转换为 DAC code。

### POINT2_FIFO_CLEAR

```text
offset  size  field
0       1     command type = 0x20
```

清空 FIFO，停止 FIFO 外部触发输出，并取消已准备的单点脉冲。成功返回 `ACK / DONE / NONE`。

### POINT2_FIFO_APPEND

```text
offset  size  field
0       1     command type = 0x21
1       2     point_count, uint16 little-endian
3       8*N   N * (ch1 voltage float32_le, ch2 voltage float32_le)
```

单帧最多追加 `500` 个点，这是由当前协议最大 payload `4004 bytes` 决定的。若 FIFO 剩余空间不足以完整写入这一帧，固件返回 `ACK / BUSY / NONE`，不会写入这一批点；上位机应稍后重试。

### POINT2_FIFO_START

```text
offset  size  field
0       1     command type = 0x22
```

开启 FIFO 外部触发输出。开启后每个 `EXT_PULSE_IN` 上升沿：

```text
如果 FIFO 非空：pop 一个点 -> 更新 CH1/CH2 DAC -> 启动 TIM4/TIM5 one-pulse
如果 FIFO 为空：忽略该外部触发
```

### POINT2_FIFO_STOP

```text
offset  size  field
0       1     command type = 0x23
```

停止 FIFO 外部触发输出并清空 FIFO。这样 stop 后不会残留旧轨迹。

POINT FIFO 和 POINT2 FIFO 互斥。任一 FIFO active 期间，`POINT_SET`、`POINT2_SET`、`POINT_PULSE_WIDTH_SET`、`SEQUENCE_SET`、`SYNC`、`WAVEFORM_CONFIG` 和 `TIMER_SET` 返回 `ACK / BUSY / NONE`。如需切换模式，先发送对应 FIFO 的 `STOP` 或 `CLEAR`。

## POINT_PULSE_WIDTH_SET

用途：设置单点模式在外部触发到达后输出的脉冲宽度。默认值为 `10 us`。

```text
offset  size  field
0       1     command type = 0x50
1       4     pulse_width_us, float32 little-endian, unit = us
```

脉宽范围：

```text
1..1000000 us
```

若脉宽为 `NaN`、`Inf`、小于 `1us` 或大于 `1s`，固件返回 `NACK / OUT_OF_RANGE`。如果已有 pending 或 firing 的单点脉冲，则返回 `ACK / BUSY / NONE`。

当前外部脉冲信号约为 `5 kHz`，周期约为 `200 us`。这一版固件不限制单点脉宽和外部周期之间的关系，用户应避免设置超过外部周期或系统可接受范围的脉宽。固件这一版也不做输入毛刺过滤，`EXT_PULSE_IN` 的外部信号应保证干净的上升沿。

## SEQUENCE_SET

用途：写入某个通道的一周期波形。每帧只操作一个通道。上位机推荐先发送 `WAVEFORM_CONFIG` 设置当前通道的波形长度和频率，再发送 `SEQUENCE_SET`。

```text
offset  size      field
0       1         command type = 0x60
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
60 02 02 00
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

除 `POINT_SET_NO_ACK` / `POINT2_SET_NO_ACK` 外，上位机每发送一个命令帧后，应等待 ACK/NACK，再发送下一帧。高频单点扫描可以使用 NO_ACK 单点命令，但主机端将无法逐帧确认 BUSY、DONE 或运行期错误。推荐使用 `test/wgs_matlab` 中的 MATLAB client：

```matlab
client = wgs.connect("COM43");
cleanupObj = onCleanup(@() client.close());

client.sequenceSet(1, ch1Samples, 100);  % config length/frequency first
client.sequenceSet(2, ch2Samples, 100);
```
