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
  wgs_point_sine_demo.m   POINT_SET for 循环正弦演示
  test_wgs_protocol.m     无需硬件的帧构造测试
```

普通 MATLAB 脚本通常只需要使用 `wgs.Client`、`wgs.connect` 和 `wgs.printReply`。
`wgs.internal` 包保存协议细节，因此 demo 和光学设计脚本不需要了解具体字节布局。

## 快速开始

确保 `+wgs` 的父目录已经加入 MATLAB 路径：

```matlab
addpath("test/wgs_matlab")
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
reply = client.pointSet(2, 1.65, 50.0);    % CH2 保持 1.65 V，ACK 脉宽 50 us
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

POINT_SET:
  payload = 10 channel float32_le(voltage) float32_le(ack_pulse_us)

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

这个脚本逐点发送 `POINT_SET` 控制 CH2，适合验证单点保持模式和串口应答链路。它不是高频波形方案；高频或稳定周期波形应使用 `sequenceSet()`。

`wgs_point_sine_demo` 使用 `client.pointSetNoReply()` 提高逐点发送速度。这个接口只发送命令帧，不等待 ACK/NACK；固件仍会回 ACK，因此脚本在回到普通带应答命令前会调用 `client.flushInput()` 清理积压回复。
