## 目录导航

- [1.Firmware](1.Firmware/)：STM32 固件工程。
  - [WGS 固件模块说明](1.Firmware/user/app/WGS/README.md)
  - [RTT 调试组件说明](1.Firmware/user/lib/RTT/README.md)
  - [SystemView 调试组件说明](1.Firmware/user/lib/systemview/README.md)
- [2.TestForHost](2.TestForHost/)：上位机测试与调用工具。
  - [MATLAB 接口说明](2.TestForHost/wgs_matlab/README.md)
  - [Python GUI 上位机说明](2.TestForHost/wgs_python/README.md)

## 环境需求

1. cmake >= 3.24
2. ninja >= 1.10
3. [arm-none-eabi-toolchain](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads)
4. python >= 3.10
5. MATLAB with Instrument Control Toolbox
6. Python packages in [2.TestForHost/wgs_python/requirements.txt](2.TestForHost/wgs_python/requirements.txt)

推荐安装以下软件，以更好开发本项目:

1. STM32CubeMX
2. OpenOCD
3. [cortex-debug](https://github.com/Marus/cortex-debug) or STM32CubeIDE
4. clangd
5. clang-format
6. serial debug assistant

## 快速开始

1. 编译并下载 [1.Firmware](1.Firmware/) 固件到 STM32。
2. 按下表连接上位机、STM32 和外部测量/执行设备。
3. 使用串口调试助手，或启动 [Python GUI 上位机](2.TestForHost/wgs_python/README.md) 连接开发板。
4. 在 MATLAB 设计脚本中调用 [MATLAB 接口](2.TestForHost/wgs_matlab/README.md) 发送单点电压、周期波形或采样率配置。

### 硬件连接

上位机通过 USB-TTL 串口模块连接 STM32 的 `USART1`。串口参数为 `921600, 8N1`，不使用硬件流控。

```text
USB-TTL TXD  -> STM32 PA10 / USART1_RX
USB-TTL RXD  <- STM32 PA9  / USART1_TX
USB-TTL GND  <-> STM32 GND
```

DAC 输出和 ACK 脉冲引脚如下：

| 功能 | STM32 引脚 | 说明 |
|---|---:|---|
| CH1 DAC 输出 | `PA4 / DAC_OUT1` | 输出范围约 `0..3.3 V` |
| CH2 DAC 输出 | `PA5 / DAC_OUT2` | 输出范围约 `0..3.3 V` |
| CH1 ACK1 | `PD12 / TIM4_CH1` | CH1 电压更新/单点命令 ACK 脉冲 |
| CH1 ACK2 | `PD13 / TIM4_CH2` | 与 CH1 ACK1 同时输出 |
| CH2 ACK1 | `PA2 / TIM5_CH3` | CH2 电压更新/单点命令 ACK 脉冲 |
| CH2 ACK2 | `PA3 / TIM5_CH4` | 与 CH2 ACK1 同时输出 |
| EXT_PULSE_IN 触发输入 | `PB0 / EXT_PULSE_IN` | 外部脉冲 |
| Buzzer | `PC0` | 收到协议内命令后短鸣 |

接线注意事项：

- USB-TTL 模块必须使用 `3.3 V TTL` 电平；不要把 RS-232 电平或 `5 V` 推挽信号直接接入 STM32 IO。
- USB-TTL、STM32、示波器、后级执行器需要共地，否则 DAC 电压和 ACK 脉冲的参考电平不可靠。
- DAC 输出适合接高阻输入或后级缓冲/驱动电路，不建议直接驱动低阻负载。
- ACK 是 `3.3 V` 数字脉冲；若接到外部逻辑、光耦或比较器，请确认输入阈值和负载不会钳位 MCU 引脚。
