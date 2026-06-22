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
2. 使用串口调试助手，或启动 [Python GUI 上位机](2.TestForHost/wgs_python/README.md) 连接开发板。
3. 在matlab设计脚本中调用 [MATLAB 接口](2.TestForHost/wgs_matlab/README.md) 发送单点电压、周期波形或采样率配置。
