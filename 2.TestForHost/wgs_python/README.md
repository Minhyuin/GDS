# WGS Python Host

WGS Python Host 是用于调试和使用 Waveform Generation System 固件的电脑端工具。它通过串口发送 WGS 协议帧，可以完成连接测试、单点电压输出、1000 点周期波形下发、TIM6/TIM7 触发频率配置，并提供波形预览和 TX/RX 十六进制日志。

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
- Baud: 默认 `115200`
- DTR/RTS: 程序会在打开串口后置为 `False`，避免 CH340 打开串口时复位或干扰单片机状态

## GUI 功能

- `Refresh`: 重新扫描串口
- `Connect`: 打开或关闭串口
- `Ping`: 发送 `PING`
- `Point Set`: 对 CH1 或 CH2 直接输出目标电压
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

SEQUENCE_SET:
  20 channel uint16_le(sample_count) sample_count * float32_le(voltage)

TIMER_SET:
  40 channel float32_le(sample_rate_hz)
```

通道：

```text
CH1 = 0x01  DAC_OUT1 / TIM6
CH2 = 0x02  DAC_OUT2 / TIM7
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
- `TIMER_SET CH2 100kHz` 帧
- `SEQUENCE_SET CH2 1000 points` 帧长度和头部

## Python 调用示例

```python
from source.modules.client import WgsClient

client = WgsClient("COM43", 115200)
client.open()
try:
    print(client.ping().summary())
    print(client.point_set("CH2", 1.65).summary())
    print(client.timer_set("CH2", 10000.0).summary())
    print(client.sequence_set("CH2", [1.65] * 1000).summary())
finally:
    client.close()
```

## 注意事项

- `SEQUENCE_SET` 最大 1000 点；固件会把不足 1000 点的周期缓冲区用最后一个点补齐。
- 电压值建议控制在 0.0 到 3.3V。当前固件侧会钳位，GUI 侧也限制输入范围。
- 若切换到高波形频率，需要同时考虑 DAC 建立时间、DMA、定时器时钟、运放/振镜驱动带宽和示波器触发设置。
