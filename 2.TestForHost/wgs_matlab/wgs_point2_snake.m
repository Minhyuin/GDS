clc;
clear;
% WGS_POINT2_SNAKE Serpentine scan using one POINT2_SET_NO_ACK per point.
%
% This script keeps the original non-batched sending style: MATLAB writes one
% POINT2_SET_NO_ACK frame for each X/Y point. For higher point rates, prefer
% wgs_point2_batch_snake or wgs_point2_fifo_snake.
%
% Usage:
%   wgs_point2_snake
%   wgs_point2_snake("COM43")

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
pointsPerRow = 200;
rows = 200;

% Target host update rate for point commands. MATLAB per-point writes are often
% limited around 1 kHz on a normal serialport path.
pointRateHz = 1000;

% Point pulse width for each POINT2_SET command in microseconds.
ackPulseUs = 10.0;

client = wgs.connect(port, "BaudRate", 921600, "Timeout", 5.0, ...
                     "Verbose", false, "MaxAttempts", 1);
cleanupObj = onCleanup(@() client.close());

fprintf("WGS non-batched serpentine scan on %s\n", client.Port);
fprintf("X range: %.2f..%.2f V, Y range: %.2f..%.2f V\n", ...
        xMinVoltage, xMaxVoltage, yMinVoltage, yMaxVoltage);
fprintf("Grid: %d rows x %d points/row, target %.0f points/s\n", ...
        rows, pointsPerRow, pointRateHz);

reply = client.ping();
wgs.printReply("PING", reply);

pause(0.05);

reply = client.pointPulseWidthSet(ackPulseUs);
wgs.printReply("POINT_PULSE_WIDTH_SET", reply);

% Prepare row coordinates for serpentine motion.
xLine = linspace(xMinVoltage, xMaxVoltage, pointsPerRow);
xLineReverse = fliplr(xLine);

totalCommands = rows * pointsPerRow;
pointInterval = 1.0 / pointRateHz;
sentCount = 0;

% Start at the top-left corner. POINT2_SET maps CH1 to X and CH2 to Y.
reply = client.point2Set(xLine(1), yMaxVoltage);
wgs.printReply("INIT XY", reply);
pause(0.05);

printScanProgress(0, totalCommands);
startTime = tic;
nextSendTime = 0.0;

for row = 1:rows
    yPosition = yMaxVoltage - (row - 1) * (yMaxVoltage - yMinVoltage) / max(rows - 1, 1);

    if mod(row, 2) == 1
        xPositions = xLine;
    else
        xPositions = xLineReverse;
    end

    for idx = 1:numel(xPositions)
        elapsed = toc(startTime);
        waitSeconds = nextSendTime - elapsed;
        if waitSeconds > 0
            pause(waitSeconds);
        end

        client.point2SetNoReply(xPositions(idx), yPosition);
        sentCount = sentCount + 1;
        nextSendTime = nextSendTime + pointInterval;
        printScanProgress(sentCount, totalCommands);
    end
end

elapsedSeconds = toc(startTime);
actualRateHz = sentCount / max(elapsedSeconds, eps);
fprintf("Scan completed: %d commands sent, elapsed %.2f s, actual %.1f cmds/s\n", ...
        sentCount, elapsedSeconds, actualRateHz);

pause(0.1);
reply = client.ping();
wgs.printReply("PING", reply);

disp("Non-batched serpentine scan finished.");

function printScanProgress(sentCount, totalCount)
    persistent lastProgressStep

    if sentCount == 0
        lastProgressStep = 0;
        return;
    end

    progressStep = floor(sentCount * 10 / totalCount);
    if progressStep > lastProgressStep
        lastProgressStep = progressStep;
        fprintf("Scan progress: %3.0f%% (%d/%d commands sent)\n", ...
                progressStep * 10, sentCount, totalCount);
    end

    if sentCount >= totalCount
        lastProgressStep = 0;
    end
end
