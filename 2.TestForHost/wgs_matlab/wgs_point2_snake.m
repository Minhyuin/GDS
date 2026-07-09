clc;
clear;
% TEST Laser serpentine scan from top to bottom using WGS DAC outputs.
%
% This script drives X/Y channels in a serpentine raster pattern. The laser
% itself should be enabled separately; this script only controls the scan
% coordinates through the WGS UART client.
%
% Usage:
%   test
%   test("COM43")

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

% Target host update rate for point commands. Lower this to slow scan and increase
% the time interval between each point command.
pointRateHz = 1000;
% Point pulse width for each POINT2_SET command in microseconds.
ackPulseUs = 10.0;

client = wgs.connect(port, "BaudRate", 921600, "Timeout", 5.0, ...
                     "Verbose", false, "MaxAttempts", 1);
cleanupObj = onCleanup(@() client.close());

fprintf("WGS serpentine scan on %s\n", client.Port);
fprintf("X range: %.2f..%.2f V, Y range: %.2f..%.2f V\n", ...
        xMinVoltage, xMaxVoltage, yMinVoltage, yMaxVoltage);
fprintf("Grid: %d rows × %d points/row, target %.0f points/s\n", ...
        rows, pointsPerRow, pointRateHz);

reply = client.ping();
wgs.printReply("PING", reply);

pause(0.05);

reply = client.pointPulseWidthSet(ackPulseUs);
wgs.printReply("POINT_PULSE_WIDTH_SET", reply);

% Prepare row coordinates for serpentine motion.
xLine = linspace(xMinVoltage, xMaxVoltage, pointsPerRow);
xLineReverse = fliplr(xLine);

pointPeriodSeconds = 1.0 / pointRateHz;
startTime = tic;
commandCount = 0;
totalCommands = rows * pointsPerRow;
lastProgressStep = 0;

% Start at the top-left corner. POINT2_SET maps CH1 to X and CH2 to Y.
reply = client.point2Set(xLine(1), yMaxVoltage);
wgs.printReply("INIT XY", reply);
pause(0.05);

for row = 1:rows
    yPosition = yMaxVoltage - (row - 1) * (yMaxVoltage - yMinVoltage) / max(rows - 1, 1);

    if mod(row, 2) == 1
        xPositions = xLine;
    else
        xPositions = xLineReverse;
    end

    for idx = 1:numel(xPositions)
        client.point2SetNoReply(xPositions(idx), yPosition);

        commandCount = commandCount + 1;
        progressStep = floor(commandCount * 10 / totalCommands);
        if progressStep > lastProgressStep
            lastProgressStep = progressStep;
            fprintf("Scan progress: %3.0f%% (%d/%d commands sent)\n", ...
                    progressStep * 10, commandCount, totalCommands);
        end

        scheduleTime = commandCount * pointPeriodSeconds;
        while toc(startTime) < scheduleTime
            pause(0.0005);
        end
    end
end

elapsed = toc(startTime);
actualRate = commandCount / max(elapsed, eps);
fprintf("Scan completed: %d commands sent, elapsed %.2f s, actual %.1f cmds/s\n", ...
        commandCount, elapsed, actualRate);

pause(0.1);
reply = client.ping();
wgs.printReply("PING", reply);

disp("Serpentine scan finished.");
