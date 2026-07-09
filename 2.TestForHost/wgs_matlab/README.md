# WGS MATLAB 接口

此文件夹包含 WGS 的 MATLAB 端客户端。WGS 是 Waveform Generation System
（波形生成系统）的缩写。它用于被其他设计脚本直接调用，将电压单点、周期波形和通道采样率发送给 STM32。

## 目录结构

```text
wgs_matlab/
  +wgs/
    Client.m              高层 UART 客户端类
    connect.m             便捷构造函数
    printReply.m          ACK/NACK 摘要打印工具
    +internal/            帧协议辅助函数
  wgs_uart_demo.m         端到端硬件演示
  wgs_point_sine_demo.m   POINT2_SET for 循环正弦演示
  test_wgs_protocol.m     无需硬件的帧构造测试
```

普通 MATLAB 脚本通常只需要使用 `wgs.Client`、`wgs.connect` 和 `wgs.printReply`。
`wgs.internal` 包保存协议细节，因此 demo 和光学设计脚本不需要了解具体字节布局。

## 快速开始

确保 `+wgs` 的父目录已经加入 MATLAB 路径：

```matlab
addpath("2.TestForHost/wgs_matlab")
```

打开串口客户端：

```matlab
client = wgs.connect("COM43");
cleanupObj = onCleanup(@() client.close());
```

发送 ping：

```matlab
reply = client.ping();
wgs.printReply("PING", reply);
```

直接设置通道采样率：

```matlab
reply = client.setSampleRate(1, 100000);   % CH1 / TIM4 = 100 kSamples/s
wgs.printReply("CH1 sample rate", reply);

reply = client.setSampleRate(2, 100000);   % CH2 / TIM5 = 100 kSamples/s
wgs.printReply("CH2 sample rate", reply);
```

如果更关心波形重复频率，可以使用 `sequenceSet(channel, samples, frequencyHz)`；客户端会先用样本数量配置波形长度和频率，再发送序列内容。

```matlab
reply = client.sequenceSet(1, ch1, 100);   % CH1 waveform = 100 Hz
```

采样率由波形长度和频率自动计算：

```text
sample_rate_hz = length(samples) * waveform_frequency_hz
```

输出单通道恒定电压：

```matlab
reply = client.pointSet(1, 1.65);          % CH1 保持 1.65 V
reply = client.point2Set(1.65, 2.50);      % 同时设置 CH1 / CH2
reply = client.pointPulseWidthSet(50.0);   % 单点模式脉宽 50 us
```

输出单通道周期波形：

```matlab
t = linspace(0, 2*pi, 1000).';
ch1 = 1.65 + 1.40 * sin(t);
ch2 = 1.65 + 1.40 * cos(t);

reply = client.sequenceSet(1, ch1, 100);
wgs.printReply("CH1 sequence", reply);

reply = client.sequenceSet(2, ch2, 100);
wgs.printReply("CH2 sequence", reply);
```

`sequenceSet(channel, samples, frequencyHz)` 每次只操作一个通道。`samples` 是以伏特为单位的一维数组，当前固件接受 1 到 1000 个样本；客户端会先配置 `length(samples)` 和 `frequencyHz`，且要求 `length(samples) * frequencyHz <= 100000 Sa/s`。如果裸序列帧长度超过固件当前波形长度，固件会截断；不足当前波形长度时，固件会用最后一个样本补齐。超出 `0.0..3.3 V` 的电压由固件限幅。

通道参数可以写成：

```matlab
1, 2, "CH1", "CH2", "X", "Y"
```

## 构造参数

`wgs.connect(port, name, value, ...)` 会将选项转发给 `wgs.Client`。

```matlab
client = wgs.connect("COM43", ...
    "BaudRate", 921600, ...
    "Timeout", 5.0, ...
    "Verbose", true, ...
    "MaxAttempts", 3);
```

支持的选项：

```text
BaudRate                UART 波特率，默认 921600
Timeout                 回复超时时间，单位为秒，默认 5.0
OpenStabilizeSeconds    打开 COM 端口后的静默等待时间，默认 2.0
MaxAttempts             回复超时时的命令重试次数，默认 3
Verbose                 打印 TX/RX 帧，默认 true
SafeDtr                 打开端口后的 DTR 状态，默认 false
SafeRts                 打开端口后的 RTS 状态，默认 false
```

## 接口函数参考

普通脚本只应调用 `+wgs` 包顶层函数和 `wgs.Client` 的 public 方法。`wgs.internal.*` 是协议实现细节，不建议在实验脚本中直接依赖。

### 连接和资源管理

| 函数 | 形式 | 返回 | 说明 |
|---|---|---|---|
| `wgs.connect` | `client = wgs.connect(port, name, value, ...)` | `wgs.Client` | 打开串口并返回客户端对象。`port` 为空或省略时使用 `"COM43"`。name-value 选项见“构造参数”。 |
| `wgs.Client` | `client = wgs.Client(port, name, value, ...)` | `wgs.Client` | 类构造函数。通常优先使用 `wgs.connect()`。 |
| `client.close` | `client.close()` | 无 | 关闭串口对象，并把 DTR/RTS 置为安全状态。建议配合 `onCleanup` 使用。 |
| `delete` | `delete(client)` | 无 | MATLAB 删除对象时会自动调用 `close()`。 |
| `client.flushInput` | `client.flushInput()` | 无 | 清空串口输入缓冲。一般用于手动调试或丢弃旧数据。 |

推荐写法：

```matlab
client = wgs.connect("COM43");
cleanupObj = onCleanup(@() client.close());
```

### 命令函数

所有带返回值的命令函数都会发送一帧 `COMMAND`，等待固件 `ACK` 或 `NACK`，并返回 `reply` 结构体。`reply` 的常用字段如下：

```text
reply.isAck        true 表示帧类型为 ACK
reply.isNack       true 表示帧类型为 NACK
reply.status       原始 status 字节
reply.detail       原始 detail/error 字节
reply.statusName   status 文本，例如 DONE / ERROR
reply.detailName   detail 文本，例如 NONE / INVALID_STATE
```

| 函数 | 形式 | 参数 | 返回 | 注意事项 |
|---|---|---|---|---|
| `client.ping` | `reply = client.ping()` | 无 | `reply` | 通信自检命令，不改变 DAC 输出。 |
| `client.sync` | `reply = client.sync()` | 无 | `reply` | 同步 CH1/CH2 序列相位。仅当 CH1 和 CH2 都已处于序列播放状态时有效；否则固件返回 `NACK / INVALID_STATE`。 |
| `client.pointSet` | `reply = client.pointSet(channel, voltage)` | `channel`, `voltage` | `reply` | 单点模式。停止对应通道 DMA，使该通道保持目标电压。 |
| `client.pointSetNoReply` / `client.pointSetNoAck` | `client.pointSetNoReply(channel, voltage)` | `channel`, `voltage` | 无 | 发送 `POINT_SET_NO_ACK = 0x12`。固件执行同样的单点逻辑，但有效命令不返回 ACK/BUSY/NACK。 |
| `client.point2Set` | `reply = client.point2Set(ch1Voltage, ch2Voltage)` | `ch1Voltage`, `ch2Voltage` | `reply` | 同一帧同时输出 CH1 / CH2 的目标电压。 |
| `client.point2SetNoReply` / `client.point2SetNoAck` | `client.point2SetNoReply(ch1Voltage, ch2Voltage)` | `ch1Voltage`, `ch2Voltage` | 无 | 发送 `POINT2_SET_NO_ACK = 0x13`。适合高频双通道单点扫描，但不会逐帧确认固件状态。 |
| `client.point2SetNoReplyBatch` / `client.point2SetNoAckBatch` | `stats = client.point2SetNoReplyBatch(ch1Voltages, ch2Voltages, name, value, ...)` | `ch1Voltages`, `ch2Voltages` | `stats` | 批量预构建连续 `POINT2_SET_NO_ACK` 帧，并按批次写入串口，避免 MATLAB 每点一次 `write()` 的约 1 kHz 调用瓶颈。 |
| `client.pointPulseWidthSet` | `reply = client.pointPulseWidthSet(pulseWidthUs)` | `pulseWidthUs` | `reply` | 单点模式脉宽独立设置，允许范围为 `1..1000000`，默认值为 `10 us`。 |
| `client.waveformConfig` | `reply = client.waveformConfig(channel, lengthSa, frequencyHz)` | `channel`, `lengthSa`, `frequencyHz` | `reply` | 配置通道波形长度和波形频率。`lengthSa` 必须为 `1..1000` 的整数，且 `lengthSa * frequencyHz <= 100000 Sa/s`。 |
| `client.sequenceSet` | `reply = client.sequenceSet(channel, samples, frequencyHz)` | `channel`, `samples`, `frequencyHz` | `reply` | 推荐的序列发送入口。客户端会先调用 `waveformConfig(channel, numel(samples), frequencyHz)`，成功后再发送 `samples`。 |
| `client.setSampleRate` | `reply = client.setSampleRate(channel, sampleRateHz)` | `channel`, `sampleRateHz` | `reply` | 直接设置 DAC 更新采样率。一般优先使用 `sequenceSet()` 或 `waveformConfig()`，避免长度和频率概念脱节。 |
| `client.setWaveFrequency` | `reply = client.setWaveFrequency(channel, waveformFrequencyHz)` | `channel`, `waveformFrequencyHz` | `reply` | 旧的便捷接口，按固定 `1000` 点周期换算采样率：`sampleRateHz = waveformFrequencyHz * 1000`。动态长度场景建议改用 `waveformConfig()` 或 `sequenceSet()`。 |
| `wgs.printReply` | `wgs.printReply(label, reply)` | `label`, `reply` | 无 | 打印紧凑的 `ACK/NACK status/detail` 摘要。 |

### 参数说明

`channel` 支持以下写法：

```matlab
1, 2, "CH1", "CH2", "CHANNEL1", "CHANNEL2", "X", "Y"
```

通道对应关系：

```text
CH1 / X -> DAC_OUT1 / PA4 / TIM4
CH2 / Y -> DAC_OUT2 / PA5 / TIM5
```

`voltage` 和 `samples` 的单位均为伏特。固件会把电压线性映射到 12 位 DAC code，并把超出 `0.0..3.3 V` 的值限幅到有效范围。为了让实验结果可预期，建议在 MATLAB 侧自行限制电压范围，不要依赖固件限幅作为正常控制逻辑。

`samples` 可以是行向量或列向量，客户端会内部 reshape 成一维数组。当前固件每通道最大波形长度为 `1000 Sa`。

`frequencyHz` 是整个波形周期的重复频率，不是单个采样点的更新频率。实际采样率为：

```text
sampleRateHz = numel(samples) * frequencyHz
```

`pointPulseWidthSet()` 只影响单点模式的脉冲宽度；序列模式 ACK 脉宽由采样周期的一半决定，例如 `100 kSa/s` 时采样周期为 `10 us`，ACK 脉宽约为 `5 us`。

### 调用顺序建议

- 单点保持：直接调用 `pointSet()`。
- 双通道单点输出：调用 `point2Set()`。
- 单点脉宽独立设置：调用 `pointPulseWidthSet()`。
- 周期波形：优先调用 `sequenceSet(channel, samples, frequencyHz)`，让客户端先配置长度/频率再发送序列。
- 双通道同相启动：先分别对 CH1、CH2 调用 `sequenceSet()`，确认都返回 ACK 后，再调用 `sync()`。
- 高速单点扫描：使用 `pointSetNoReply()` / `point2SetNoReply()` 发送真正的 NO_ACK 单点命令，避免固件 ACK 流量积压。
- 高频双通道逐点扫描：优先使用 `point2SetNoReplyBatch()`。它先把点列编码成连续帧，再按批次 `write()`，可选 `"PointRateHz"`、`"BatchPointCount"` 和 `"ProgressFcn"`。
- 高速或长序列发送时，不要并发操作同一个串口；每条带应答命令应等待返回后再发送下一条。

批量单点发送示例：

```matlab
stats = client.point2SetNoReplyBatch( ...
    xVoltages, yVoltages, ...
    "PointRateHz", 5000, ...
    "BatchPointCount", 500, ...
    "ProgressFcn", @(sent, total) fprintf("%d/%d\n", sent, total));

fprintf("actual %.1f cmds/s\n", stats.actualRateHz);
```

## DTR/RTS 注意事项

CH340 转接器可能会暴露 DTR/RTS 调制解调器控制线。在某些开发板上，这些控制线会通过自动下载电路连接到 `NRST` 或 `BOOT0`。MATLAB 的 `serialport()` 在打开端口时可能会切换这些控制线，从而导致 STM32 复位或进入系统 bootloader。

因此，客户端会在打开串口后立即设置：

```matlab
setDTR(dev, false);
setRTS(dev, false);
```

并在关闭时应用相同的安全状态。如果开发板仍然在端口打开的一瞬间复位，请在硬件上断开 DTR 和 RTS 与 `NRST`/`BOOT0` 的连接。

## 协议摘要

帧格式：

```text
SOF      1 byte   0xA5
type     1 byte
length   2 bytes  uint16 小端序 payload 长度
payload  N bytes
```

帧类型：

```text
0x01 COMMAND
0x02 ACK
0x03 NACK
```

命令：

```text
PING:
  payload = 00

SYNC:
  payload = 01

POINT_SET:
  payload = 10 channel float32_le(voltage)

POINT_SET_NO_ACK:
  payload = 12 channel float32_le(voltage)

POINT2_SET:
  payload = 11 float32_le(ch1_voltage) float32_le(ch2_voltage)

POINT2_SET_NO_ACK:
  payload = 13 float32_le(ch1_voltage) float32_le(ch2_voltage)

POINT_PULSE_WIDTH_SET:
  payload = 50 float32_le(pulse_width_us)

SEQUENCE_SET:
  payload = 20 channel uint16_le(sampleCount)
            sampleCount * float32_le(voltage)

WAVEFORM_CONFIG:
  payload = 30 channel uint16_le(length_sa) float32_le(frequency_hz)

TIMER_SET:
  payload = 40 channel float32_le(sample_rate_hz)
```

通道：

```text
0x01 CH1 / DAC_OUT1 / TIM4
0x02 CH2 / DAC_OUT2 / TIM5
```

当前固件的成功回复格式类似于：

```text
A5 02 02 00 01 00
```

含义为 `ACK`、payload 长度为 `2`、状态为 `DONE`、详情为 `NONE`。

## 测试和演示

运行无需硬件的帧构造测试：

```matlab
test_wgs_protocol
```

运行端到端 UART demo：

```matlab
wgs_uart_demo
```

运行单点模式 for 循环正弦 demo：

```matlab
wgs_point_sine_demo
```

这个脚本逐点发送 `POINT2_SET_NO_ACK`，在目标通道输出正弦电压，同时让另一通道保持中心电压，适合验证高频单点发送路径。它不是稳定周期波形方案；稳定周期波形应使用 `sequenceSet()`。

`wgs_point_sine_demo` 使用 `client.point2SetNoReply()` 提高逐点发送速度。这个接口发送 `POINT2_SET_NO_ACK` 命令帧；有效命令不会产生固件 ACK，因此回到普通带应答命令前通常不需要清理积压回复。
