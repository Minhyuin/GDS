# WGS MATLAB Interface

This folder contains the MATLAB-side client for the WGS, short for
Waveform Generation System. It is intended to be called directly from optical
design scripts that produce galvanometer voltage points or voltage waveforms.

## Layout

```text
test/matlab/
  +wgs/
    Client.m              High-level UART client class
    connect.m             Convenience constructor
    printReply.m          ACK/NACK summary printer
    +internal/            Frame protocol helpers
  wgs_uart_demo.m         End-to-end hardware demo
  wgs_open_only_dtr_test.m  Serial open/control-line diagnostic
  test_wgs_protocol.m     Hardware-free frame construction test
```

Only `wgs.Client`, `wgs.connect`, and `wgs.printReply` are meant to be used by
normal MATLAB scripts. The `wgs.internal` package holds protocol details so the
demo and optical design scripts do not need to know the byte layout.

## Quick Start

Make sure the parent folder of `+wgs` is on the MATLAB path:

```matlab
addpath("test/matlab")
```

Open the serial client:

```matlab
client = wgs.connect("COM43");
cleanupObj = onCleanup(@() client.close());
```

Send a ping:

```matlab
reply = client.ping();
wgs.printReply("PING", reply);
```

Output one galvanometer point:

```matlab
reply = client.pointSet(1.65, 1.65);
wgs.printReply("POINT_SET", reply);
```

Output a waveform sequence:

```matlab
t = linspace(0, 2*pi, 128).';
points = [
    1.65 + 1.40 * sin(t), ...
    1.65 + 1.40 * cos(t)
];

reply = client.sequenceSet(points);
wgs.printReply("SEQUENCE_SET", reply);
```

Each row of `points` is one `[xVoltage, yVoltage]` pair in volts. The current
firmware accepts 1 to 255 points per `SEQUENCE_SET` frame. Voltages outside
`0.0..3.3 V` are clamped by firmware.

## Constructor Options

`wgs.connect(port, name, value, ...)` forwards options to `wgs.Client`.

```matlab
client = wgs.connect("COM43", ...
    "BaudRate", 115200, ...
    "Timeout", 5.0, ...
    "Verbose", true, ...
    "MaxAttempts", 3);
```

Supported options:

```text
BaudRate                UART baud rate, default 115200
Timeout                 Reply timeout in seconds, default 5.0
OpenStabilizeSeconds    Quiet time after opening COM port, default 2.0
MaxAttempts             Command retry count on reply timeout, default 3
Verbose                 Print TX/RX frames, default true
SafeDtr                 DTR state after opening port, default false
SafeRts                 RTS state after opening port, default false
```

## DTR/RTS Note

The CH340 adapter may expose DTR/RTS modem-control lines. On some boards these
lines are connected to `NRST` or `BOOT0` through an auto-download circuit.
MATLAB `serialport()` can toggle them when opening the port, which may reset the
STM32 or enter the system bootloader.

The client therefore sets:

```matlab
setDTR(dev, false);
setRTS(dev, false);
```

immediately after opening the serial port, and applies the same safe state when
closing. If a board still resets at the instant the port opens, disconnect DTR
and RTS from `NRST`/`BOOT0` in hardware.

## Protocol Summary

Frame format:

```text
SOF      1 byte   0xA5
type     1 byte
length   2 bytes  uint16 little-endian payload length
payload  N bytes
```

Frame types:

```text
0x01 COMMAND
0x02 ACK
0x03 NACK
```

Commands:

```text
PING:
  payload = 00

POINT_SET:
  payload = 10 float32_le(xVoltage) float32_le(yVoltage)

SEQUENCE_SET:
  payload = 20 uint16_le(pointCount)
            pointCount * [float32_le(xVoltage), float32_le(yVoltage)]
```

Successful firmware replies currently look like:

```text
A5 02 02 00 01 00
```

which means `ACK`, payload length `2`, status `DONE`, detail `NONE`.

## Tests and Demos

Run the hardware-free frame construction test:

```matlab
test_wgs_protocol
```

Run the end-to-end UART demo:

```matlab
wgs_uart_demo("COM43")
```

Run only the DTR/RTS open-port diagnostic:

```matlab
wgs_open_only_dtr_test
```
