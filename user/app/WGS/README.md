# WGS - Waveform Generation System

WGS 是固件中的波形生成系统模块，负责接收上位机命令、解析协议帧、控制输出模式，并最终通过 DAC 输出振镜控制电压。

当前 WGS 运行在 FreeRTOS 上。串口中断或 HAL 回调只负责通知 WGS 任务，实际的数据消费、协议解析、命令处理、输出 FIFO 写入和应答发送都在 WGS worker task 中完成。DAC 真实输出由 TIM6 TRGO 触发 DAC + DMA 完成，CPU 只在 DMA 半传输/全传输回调中补充下一半播放缓冲。

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

- `BSP::UsartProxy<BSP::Usart1HandleProvider>`
- `BSP::DacProxy`
- `BSP::TimProxy<BSP::Tim6HandleProvider>`

## RTOS 行为

WGS 初始化时会创建一个静态 FreeRTOS 任务：

```cpp
xTaskCreateStatic(..., "WGS_RX", ...);
```

任务主循环阻塞等待通知：

```cpp
ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
```

没有接收事件时，WGS 任务不会轮询占用 CPU。

串口中断、UART IDLE 回调或 DMA RX 事件回调应只调用：

```cpp
notifyWgsRxFromIsr();
```

该函数会通知 WGS 任务。真正的 RX buffer 消费、协议解析和应答发送都在任务上下文中执行。

当前 `WGS.cc` 已提供 `HAL_UARTEx_RxEventCallback()`，当回调来自 `USART1` 时会调用 `notifyWgsRxFromIsr()`。需要注意：底层 transport 当前启动 RX 的接口仍是 `HAL_UART_Receive_DMA()`；如果工程希望依赖 `HAL_UARTEx_RxEventCallback()` 的 receive-to-idle 行为，后续应把 BSP UART RX 启动路径切换为 `HAL_UARTEx_ReceiveToIdle_DMA()`，或在 DMA RX complete / UART IDLE 的实际回调路径中继续只做任务通知。

## 串口传输

当前默认物理链路是 USART1 + DMA。

上位机需要发送二进制帧，不建议使用 ASCII、CSV 或文本命令作为主协议。

当前协议是小端字段为主，帧头尽量压缩以降低串口开销：

```text
SOF: 0xA5
```

## 协议帧格式

```text
+--------+---------+---------+
| SOF    | Header  | Payload |
+--------+---------+---------+

SOF:
  1 byte
  fixed: 0xA5

Header:
  type:    1 byte
  length:  2 bytes, little-endian

Payload:
  length bytes
```

完整字节布局：

```text
offset  size  field
0       1     SOF = 0xA5
1       1     frame type
2       2     payload length, little-endian
4       N     payload
```

当前版本暂不使用 CRC，也不携带 version 和 seq。CRC 模块保留在固件中，后续可通过 frame policy 重新启用。

当前最大 payload 长度：

```text
256 bytes
```

## Frame Type

```cpp
enum class FrameType_e : uint8_t {
  COMMAND = 0x01,
  ACK     = 0x02,
  NACK    = 0x03,
  EVENT   = 0x04,
  STATUS  = 0x05,
};
```

上位机发送命令时，`frame type` 应为：

```text
0x01 COMMAND
```

固件返回应答时，`frame type` 为：

```text
0x02 ACK
0x03 NACK
```

## Command Payload

命令帧 payload 的第一个字节是命令类型：

```text
payload[0] = command type
```

当前已实现：

```cpp
PING       = 0x00
POINT_SET = 0x10
SEQUENCE_SET = 0x20
```

协议已预留但尚未完成 payload 解码和行为实现：

```cpp
GET_STATUS      = 0x30
EMERGENCY_STOP  = 0x7F
```

未实现命令当前会返回：

```text
NACK / UNSUPPORTED_COMMAND
```

## PING

用途：检查通信链路和协议解析是否正常。

Payload：

```text
offset  size  field
0       1     command type = 0x00
```

示例：

```text
COMMAND frame
payload = 00
```

成功时固件返回 `ACK / DONE`。

## POINT_SET

用途：直接设置输出电压，当前用于单点模式。

Payload：

```text
offset  size  field
0       1     command type = 0x10
1       4     x voltage, float32 little-endian, unit = V
5       4     y voltage, float32 little-endian, unit = V
```

电压范围：

```text
0.0..3.3 V
```

固件会将电压线性映射到 12 位 DAC code：

```text
dac_code = round(voltage_v * 4095 / 3.3)
```

若任一通道电压为 `NaN`、`Inf`、负数或超过 `3.3V`，固件会钳位到 `0.0..3.3V` 范围内，不返回范围错误。

示例：设置 X=1.65V，Y=3.30V：

```text
payload:
10 33 33 D3 3F 33 33 53 40

解释:
10            POINT_SET
33 33 D3 3F   x = float32 little-endian 1.65V
33 33 53 40   y = float32 little-endian 3.30V
```

成功时固件返回 `ACK / DONE`。

## SEQUENCE_SET

用途：一次发送多个输出点，提高串口帧的有效数据效率。序列模式和单点模式共用同一个输出 FIFO，收到的数据会先转换为 12 位 DAC code，再进入缓冲区。DAC 播放器使用 circular DMA 双半缓冲；TIM6 只作为 DAC 触发定时器，DMA half/full 回调负责从 FIFO 补料。

Payload：

```text
offset  size        field
0       1           command type = 0x20
1       1           point count N
2       N * 8       repeated points
```

单个点格式：

```text
offset  size  field
0       4     x voltage, float32 little-endian, unit = V
4       4     y voltage, float32 little-endian, unit = V
```

当前约束：

```text
1 <= N <= 31
payload length = 2 + N * 8
voltage clamp range = 0.0..3.3V
```

如果 `N = 0`、`N > 31` 或 payload 长度不匹配，固件返回 `NACK / BAD_LENGTH`。

如果共享输出 FIFO 剩余空间不足以放下本帧全部点，固件不会做部分写入，而是返回：

```text
NACK / BUFFER_FULL
```

当 DAC DMA 补料速度快于上位机写入速度，导致 FIFO 为空时，固件用上一次输出值填充 DMA 播放缓冲，不主动清零、不停止 TIM6。

示例：一次发送两个点 `(1.65V, 1.65V)` 和 `(0.0V, 3.3V)`：

```text
payload:
20 02
33 33 D3 3F 33 33 D3 3F
00 00 00 00 33 33 53 40
```

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

成功应答：

```text
frame type = ACK
payload    = 01 00
```

失败应答：

```text
frame type = NACK
payload    = 80 error_code
```

## 上位机发送流程

上位机建议按以下流程通信：

```text
1. 组帧
2. 发送 COMMAND frame
3. 等待 ACK 或 NACK
4. 若 NACK，根据 detail 字段判断错误原因
5. 若超时，可重发或重新同步
```

由于当前帧头不含 `seq`，上位机应采用一帧一应答的顺序通信策略：发送下一帧前先等待上一帧的 ACK/NACK，避免多帧并发时无法匹配应答。

收到长度错误或未知命令时，固件会尝试返回 NACK。若 SOF 丢失或帧损坏严重，解析器会重新寻找下一个 SOF。

## MATLAB / PC 端组帧伪代码

```text
payload = [command bytes...]

header:
  type    = COMMAND
  length  = uint16 little-endian payload.length

frame = [
  0xA5,
  header,
  payload
]
```

## 当前能力状态

已实现：

- USART1 DMA RX buffer 启动
- WGS 静态 FreeRTOS worker task
- ISR 通知入口 `notifyWgsRxFromIsr()`
- `USART1` 的 `HAL_UARTEx_RxEventCallback()` 通知入口
- 阻塞等待接收事件
- RX 数据消费
- 协议帧解析
- `PING`
- `POINT_SET`
- `SEQUENCE_SET`
- ACK/NACK 应答
- POINT_SET / SEQUENCE_SET 到共享 FIFO 的写入路径
- TIM6 TRGO 触发 DAC 转换
- DAC circular DMA 双半缓冲播放
- CH1 DMA half/full 回调从 FIFO 补料，并同步填充 CH1/CH2 播放缓冲
- FIFO 为空时用上一次输出值填充 DMA 缓冲
- FIFO 空间不足时整帧拒绝并返回 `BUFFER_FULL`

尚未完成：

- BSP UART RX 启动路径与 receive-to-idle 回调机制的最终统一
- 流式播放双缓冲填充策略
- TX DMA complete 后调用 `onTxComplete()`
- 角度到电压或 DAC code 的转换
- 上位机 MATLAB SDK

## ISR 接入建议

接入 UART RX/IDLE 回调时，应保持 ISR 极短：

```cpp
extern "C" void notifyWgsRxFromIsr();

void Some_UART_Rx_Callback(...)
{
  notifyWgsRxFromIsr();
}
```

不要在 ISR 中做：

- 协议解析
- DAC 输出
- ACK/NACK 发送
- 日志打印
- 动态内存分配

这些都应留给 WGS worker task。
