clc;
clear;
%WGS_POINT_SINE_DEMO Drive CH2 with POINT_SET commands in a MATLAB loop.
%
% This demo intentionally uses POINT_SET instead of SEQUENCE_SET. It is useful
% for verifying direct DAC hold mode and host command behavior, not for high
% frequency waveform generation.

port = "COM43";
channel = 1;

centerVoltage = 1.65;
amplitudeVoltage = 1.20;
waveFrequencyHz = 1;
pointRateWavePeriods = 100;
durationSeconds = 60;
pointRateHz = pointRateWavePeriods * waveFrequencyHz;

client = wgs.connect(port, "BaudRate", 921600, "Timeout", 5.0, ...
                     "Verbose", false);
cleanupObj = onCleanup(@() client.close());

fprintf("WGS POINT_SET sine demo on %s @ %d baud\n", ...
        client.Port, client.BaudRate);
fprintf("CH%d: %.3f V center, %.3f V amplitude, %.3f Hz sine, %.1f points/s\n", ...
        channel, centerVoltage, amplitudeVoltage, waveFrequencyHz, pointRateHz);

reply = client.ping();
wgs.printReply("PING", reply);

samplePeriodSeconds = 1 / pointRateHz;
pointCount = max(1, round(durationSeconds * pointRateHz));
ticHandle = tic;

for i = 0:pointCount-1
    targetTime = i * samplePeriodSeconds;
    phase = 2 * pi * waveFrequencyHz * targetTime;
    voltage = centerVoltage + amplitudeVoltage * sin(phase);

    client.pointSetNoReply(channel, voltage);

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

reply = client.pointSet(channel, centerVoltage);
wgs.printReply("POINT_SET CH2 center", reply);

disp("POINT_SET sine demo finished.");
