clc;
clear;
% WGS_POINT2_FIFO_SNAKE Serpentine scan using MCU-side POINT2 FIFO.
%
% This script sends the X/Y path into the firmware FIFO. Each EXT_PULSE_IN
% rising edge consumes one queued point, updates both DAC channels, and emits
% one point pulse.

port = "COM43";
if exist('varargin', 'var') && ~isempty(varargin)
    port = string(varargin{1});
end

% Scan area in DAC output volts. Adjust to match your scanner / optics.
xMinVoltage = 0.05;
xMaxVoltage = 0.65;
yMinVoltage = 0.05;
yMaxVoltage = 0.65;

% Grid resolution: points per row and number of rows.
pointsPerRow = 1000;
rows = 200;

% External trigger and FIFO feed settings.
externalPulseHz = 1000;
appendPointCount = 500;
preloadPointCount = 500;

% Point pulse width in microseconds.
ackPulseUs = 10.0;

client = wgs.connect(port, "BaudRate", 921600, "Timeout", 5.0, ...
                     "Verbose", false, "MaxAttempts", 1);
cleanupObj = onCleanup(@() client.close());

fprintf("WGS POINT2 FIFO serpentine scan on %s\n", client.Port);
fprintf("X range: %.2f..%.2f V, Y range: %.2f..%.2f V\n", ...
        xMinVoltage, xMaxVoltage, yMinVoltage, yMaxVoltage);
fprintf("Grid: %d rows x %d points/row, external trigger %.0f Hz\n", ...
        rows, pointsPerRow, externalPulseHz);

reply = client.ping();
wgs.printReply("PING", reply);

pause(0.05);

reply = client.pointPulseWidthSet(ackPulseUs);
wgs.printReply("POINT_PULSE_WIDTH_SET", reply);

xLine = linspace(xMinVoltage, xMaxVoltage, pointsPerRow);
xLineReverse = fliplr(xLine);

totalCommands = rows * pointsPerRow;
xScan = zeros(1, totalCommands);
yScan = zeros(1, totalCommands);
writeIndex = 1;

for row = 1:rows
    yPosition = yMaxVoltage - (row - 1) * (yMaxVoltage - yMinVoltage) / max(rows - 1, 1);

    if mod(row, 2) == 1
        xPositions = xLine;
    else
        xPositions = xLineReverse;
    end

    nextIndex = writeIndex + numel(xPositions) - 1;
    xScan(writeIndex:nextIndex) = xPositions;
    yScan(writeIndex:nextIndex) = yPosition;
    writeIndex = nextIndex + 1;
end

printScanProgress(0, totalCommands);
progressFcn = @(sentCount, totalCount) printScanProgress(sentCount, totalCount);
stats = client.point2FifoStream( ...
    xScan, yScan, ...
    "AppendPointCount", appendPointCount, ...
    "PreloadPointCount", preloadPointCount, ...
    "StopAfterSend", false, ...
    "ProgressFcn", progressFcn);

fprintf("FIFO append completed: %d commands queued, elapsed %.2f s, actual %.1f cmds/s\n", ...
        stats.sentCount, stats.elapsedSeconds, stats.actualAppendRateHz);

drainSeconds = totalCommands / externalPulseHz;
fprintf("Waiting %.2f s for external triggers to drain FIFO...\n", drainSeconds);
pause(drainSeconds + 0.1);

reply = client.point2FifoStop();
wgs.printReply("POINT2_FIFO_STOP", reply);

reply = client.ping();
wgs.printReply("PING", reply);

disp("POINT2 FIFO serpentine scan finished.");

function printScanProgress(sentCount, totalCount)
    persistent lastProgressStep

    if sentCount == 0
        lastProgressStep = 0;
        return;
    end

    progressStep = floor(sentCount * 10 / totalCount);
    if progressStep > lastProgressStep
        lastProgressStep = progressStep;
        fprintf("FIFO append progress: %3.0f%% (%d/%d points queued)\n", ...
                progressStep * 10, sentCount, totalCount);
    end

    if sentCount >= totalCount
        lastProgressStep = 0;
    end
end
