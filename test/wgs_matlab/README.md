# WGS MATLAB 接口

此文件夹包含 WGS 的 MATLAB 端客户端。WGS 是 Waveform Generation System
（波形生成系统）的缩写。它用于被波形设计脚本直接调用，这些脚本会生成电压点或电压序列。

## 目录结构

```text
your_own_path/wgs_matlab/
  +wgs/
    Client.m              高层 UART 客户端类
    connect.m             便捷构造函数
    printReply.m          ACK/NACK 摘要打印工具
    +internal/            帧协议辅助函数
  wgs_uart_demo.m         端到端硬件演示
  wgs_open_only_dtr_test.m  串口打开/控制线诊断
  test_wgs_protocol.m     无需硬件的帧构造测试
```

普通 MATLAB 脚本通常只需要使用 `wgs.Client`、`wgs.connect` 和 `wgs.printReply`。
`wgs.internal` 包保存协议细节，因此 demo 和光学设计脚本不需要了解具体的字节布局。

## 快速开始

确保 `+wgs` 的父目录已经加入 MATLAB 路径：

```matlab
addpath("test/matlab")
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

输出一个振镜点：

```matlab
reply = client.pointSet(1.65, 1.65);
wgs.printReply("POINT_SET", reply);
```

输出一组波形序列：

```matlab
t = linspace(0, 2*pi, 128).';
points = [
    1.65 + 1.40 * sin(t), ...
    1.65 + 1.40 * cos(t)
];

reply = client.sequenceSet(points);
wgs.printReply("SEQUENCE_SET", reply);
```

`points` 的每一行都是一组以伏特为单位的 `[xVoltage, yVoltage]` 电压对。
当前固件每个 `SEQUENCE_SET` 帧接受 1 到 255 个点。超出 `0.0..3.3 V`
范围的电压会由固件进行限幅。

## 构造参数

`wgs.connect(port, name, value, ...)` 会将选项转发给 `wgs.Client`。

```matlab
client = wgs.connect("COM43", ...
    "BaudRate", 115200, ...
    "Timeout", 5.0, ...
    "Verbose", true, ...
    "MaxAttempts", 3);
```

支持的选项：

```text
BaudRate                UART 波特率，默认 115200
Timeout                 回复超时时间，单位为秒，默认 5.0
OpenStabilizeSeconds    打开 COM 端口后的静默等待时间，默认 2.0
MaxAttempts             回复超时时的命令重试次数，默认 3
Verbose                 打印 TX/RX 帧，默认 true
SafeDtr                 打开端口后的 DTR 状态，默认 false
SafeRts                 打开端口后的 RTS 状态，默认 false
```

## DTR/RTS 注意事项

CH340 转接器可能会暴露 DTR/RTS 调制解调器控制线。在某些开发板上，这些控制线会通过自动下载电路连接到
`NRST` 或 `BOOT0`。MATLAB 的 `serialport()` 在打开端口时可能会切换这些控制线，从而导致
STM32 复位或进入系统 bootloader。

因此，客户端会在打开串口后立即设置：

```matlab
setDTR(dev, false);
setRTS(dev, false);
```

并在关闭时应用相同的安全状态。如果开发板仍然在端口打开的一瞬间复位，请在硬件上断开
DTR 和 RTS 与 `NRST`/`BOOT0` 的连接。

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
  payload = 10 float32_le(xVoltage) float32_le(yVoltage)

SEQUENCE_SET:
  payload = 20 uint16_le(pointCount)
            pointCount * [float32_le(xVoltage), float32_le(yVoltage)]
```

当前固件的成功回复格式类似于：

```text
A5 02 02 00 01 00
```

含义为 `ACK`、payload 长度为 `2`、状态为 `DONE`、详情为 `NONE`。
