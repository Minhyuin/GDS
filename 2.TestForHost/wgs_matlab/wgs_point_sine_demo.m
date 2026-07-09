clc;
clear;
%WGS_POINT_SINE_DEMO Drive one channel with POINT2_SET commands in a MATLAB loop.
%
% This demo intentionally uses POINT2_SET instead of SEQUENCE_SET. It is useful
% for verifying direct DAC hold mode and host command behavior, not for high
% frequency waveform generation.

port = "COM43";
activeChannel = 2;

centerVoltage = 1.65;
amplitudeVoltage = 1.20;
waveFrequencyHz = 1;
pointRateWavePeriods = 100;
durationSeconds = 60;
pointRateHz = pointRateWavePeriods * waveFrequencyHz;

client = wgs.connect(port, "BaudRate", 921600, "Timeout", 5.0, ...
                     "Verbose", false);
cleanupObj = onCleanup(@() client.close());

fprintf("WGS POINT2_SET sine demo on %s @ %d baud\n", ...
        client.Port, client.BaudRate);
fprintf("CH%d: %.3f V center, %.3f V amplitude, %.3f Hz sine, %.1f points/s\n", ...
        activeChannel, centerVoltage, amplitudeVoltage, waveFrequencyHz, pointRateHz);

reply = client.ping();
wgs.printReply("PING", reply);

samplePeriodSeconds = 1 / pointRateHz;
pointCount = max(1, round(durationSeconds * pointRateHz));
ticHandle = tic;

for i = 0:pointCount-1
    targetTime = i * samplePeriodSeconds;
    phase = 2 * pi * waveFrequencyHz * targetTime;
    voltage = centerVoltage + amplitudeVoltage * sin(phase);

    ch1Voltage = centerVoltage;
    ch2Voltage = centerVoltage;
    if activeChannel == 1
        ch1Voltage = voltage;
    else
        ch2Voltage = voltage;
    end

    client.point2SetNoReply(ch1Voltage, ch2Voltage);

    while toc(ticHandle) < (targetTime + samplePeriodSeconds)
        pause(0.001);
    end
end

elapsedSeconds = toc(ticHandle);
actualPointRateHz = pointCount / elapsedSeconds;
fprintf("Actual point rate: %.2f points/s, elapsed %.3f s\n", ...
        actualPointRateHz, elapsedSeconds);

pause(0.1);
client.flushInput();

reply = client.point2Set(centerVoltage, centerVoltage);
wgs.printReply("POINT2_SET center", reply);

disp("POINT2_SET sine demo finished.");
