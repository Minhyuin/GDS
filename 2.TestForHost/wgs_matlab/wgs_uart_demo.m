clc;
clear;
%WGS_UART_DEMO Send example WGS protocol frames from MATLAB.
%
% Usage:
%   wgs_uart_demo("COMx")

port = "COM43";

client = wgs.connect(port, "BaudRate", 921600, "Timeout", 5.0, ...
                        "Verbose", true);
cleanupObj = onCleanup(@() client.close());

fprintf("WGS UART demo on %s @ %d baud\n", ...
        client.Port, client.BaudRate);

reply = client.ping();
wgs.printReply("PING", reply);

% reply = client.point2Set(1.65, 1.65);
% wgs.printReply("POINT2_SET center", reply);

% pause(0.05);
% reply = client.point2Set(3.3, 3.3);
% wgs.printReply("POINT2_SET high", reply);

% pause(0.05);
% reply = client.point2Set(1.65, 1.65);
% wgs.printReply("POINT2_SET center", reply);

sampleCount = 10;
waveformFrequencyHz = 1;
t = (0:sampleCount-1).' * (2*pi / sampleCount);
ch1 = 1.65 + 1.40 * sin(t);
ch2 = ch1;

% reply = client.sequenceSet(1, ch1, waveformFrequencyHz);
% wgs.printReply("SEQUENCE_SET CH1 1000 samples", reply);

reply = client.sequenceSet(2, ch2, waveformFrequencyHz);
wgs.printReply("SEQUENCE_SET CH2 1000 samples", reply);

% shortSequence = 1.65 + 1.0 * sin(linspace(0, pi, 128));
% reply = client.sequenceSet(1, shortSequence, waveformFrequencyHz);
% wgs.printReply("SEQUENCE_SET CH1 padded 128 samples", reply);

disp("Demo finished.");
