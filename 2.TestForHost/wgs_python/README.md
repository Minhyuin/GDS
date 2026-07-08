# WGS Python Host

WGS Python Host 是用于调试和使用 Waveform Generation System 固件的电脑端工具。它通过串口发送 WGS 协议帧，可以完成连接测试、单点电压输出、双通道单点输出、单点脉宽独立设置、可配置长度/频率的周期波形下发、TIM4/TIM5 触发频率配置，并提供波形预览和 TX/RX 十六进制日志。

## 目录结构

```text
wgs_python/
  main.py
  requirements.txt
  scripts/
    test_protocol.py
  source/
    app.py
    modules/
      client/
      gui/
      protocol/
      transport/
      waveform/
```

## 安装依赖

建议在虚拟环境中安装：

```powershell
cd 2.TestForHost/wgs_python
python -m venv .venv
.\.venv\Scripts\Activate.ps1
python -m pip install -r requirements.txt
```

## 启动 GUI

```powershell
cd 2.TestForHost/wgs_python
python main.py
```

典型连接参数：

- Port: 例如 `COM43`
- Baud: 默认 `921600`
- DTR/RTS: 程序会在打开串口后置为 `False`，避免 CH340 打开串口时复位或干扰单片机状态

## GUI 功能

- `Refresh`: 重新扫描串口
- `Connect`: 打开或关闭串口
- `Ping`: 发送 `PING`
- `Point Set`: 对 CH1 或 CH2 直接输出目标电压
- `Point2 Set`: 在一帧里同时输出 CH1 / CH2 的目标电压
- `Point Pulse`: 独立设置单点模式脉宽
- `Timer`: 设置通道采样率，或按波形频率自动换算为 `sample_rate = wave_frequency * 1000`
- `Sequence Set`: 生成并发送周期波形，支持 `sine/cosine/square/triangle/constant`
- 右侧曲线区域显示即将发送的波形
- 下方日志显示 TX/RX 原始帧和 ACK/NACK 解码

## 固件协议

帧格式：

```text
SOF      1 byte   0xA5
type     1 byte
length   2 bytes  uint16 little-endian
payload  N bytes
```

帧类型：

```text
COMMAND = 0x01
ACK     = 0x02
NACK    = 0x03
EVENT   = 0x04
STATUS  = 0x05
```

命令 payload：

```text
PING:
  00

POINT_SET:
  10 channel float32_le(voltage)

POINT2_SET:
  11 float32_le(ch1_voltage) float32_le(ch2_voltage)

POINT_PULSE_WIDTH_SET:
  50 float32_le(pulse_width_us)

SEQUENCE_SET:
  20 channel uint16_le(sample_count) sample_count * float32_le(voltage)

WAVEFORM_CONFIG:
  30 channel uint16_le(length_sa) float32_le(frequency_hz)

TIMER_SET:
  40 channel float32_le(sample_rate_hz)
```

通道：

```text
CH1 = 0x01  DAC_OUT1 / TIM4
CH2 = 0x02  DAC_OUT2 / TIM5
```

应答 payload：

```text
status detail
```

成功应答示例：

```text
A5 02 02 00 01 00
```

含义为 `ACK status=DONE detail=NONE`。

## 无硬件协议测试

```powershell
python scripts/test_protocol.py
```

测试会检查：

- `PING` 帧
- `POINT_SET CH1 1.5V` 帧
- `POINT2_SET CH1 1.5V CH2 2.5V` 帧
- `POINT_PULSE_WIDTH_SET 50us` 帧
- `WAVEFORM_CONFIG CH2 256Sa 12.5Hz` 帧
- `TIMER_SET CH2 100kHz` 帧
- `SEQUENCE_SET CH2 1000 points` 帧长度和头部

## Python 调用示例

```python
from source.modules.client import WgsClient

client = WgsClient("COM43", 921600)
client.open()
try:
    print(client.ping().summary())
    print(client.point_set("CH2", 1.65).summary())
    print(client.point2_set(1.65, 2.50).summary())
    print(client.point_pulse_width_set(50.0).summary())
    print(client.sequence_set("CH2", [1.65] * 256, frequency_hz=12.5).summary())
finally:
    client.close()
```

## 注意事项

- `sequence_set(channel, samples, frequency_hz)` 会先发送 `WAVEFORM_CONFIG` 配置波形长度和频率，再发送 `SEQUENCE_SET`。
- 波形长度最大 1000 Sa，采样率 `length * frequency` 最高 100000 Sa/s。
- 如果裸 `SEQUENCE_SET` 长度超过固件当前波形长度，固件会截断到当前长度；不足当前长度时会用最后一个点补齐。
- 电压值建议控制在 0.0 到 3.3V。当前固件侧会钳位，GUI 侧也限制输入范围。
- 若切换到高波形频率，需要同时考虑 DAC 建立时间、DMA、定时器时钟、运放/振镜驱动带宽和示波器触发设置。
