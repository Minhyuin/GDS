function wgs_uart_demo(port)
%WGS_UART_DEMO Send example WGS protocol frames from MATLAB.
%
% Usage:
%   wgs_uart_demo("COMx")

    if nargin < 1
        port = "COM43";
    end

    client = wgs.connect(port, "BaudRate", 115200, "Timeout", 5.0, ...
                         "Verbose", true);
    cleanupObj = onCleanup(@() client.close()); %#ok<NASGU>

    fprintf("WGS UART demo on %s @ %d baud\n", ...
            client.Port, client.BaudRate);

    reply = client.ping();
    wgs.printReply("PING", reply);

    reply = client.pointSet(1.65, 1.65);
    wgs.printReply("POINT_SET center", reply);

    pause(0.05);
    reply = client.pointSet(0.0, 3.3);
    wgs.printReply("POINT_SET corner", reply);

    pointCount = 128;
    t = linspace(0, 2*pi, pointCount).';
    points = [
        1.65 + 1.40 * sin(t), ...
        1.65 + 1.40 * sin(2 * t + pi / 2)
    ];

    reply = client.sequenceSet(points);
    wgs.printReply("SEQUENCE_SET 128 points", reply);

    sendMaxFrameExample = false;
    if sendMaxFrameExample
        pointCount = 255;
        t = linspace(0, 2*pi, pointCount).';
        points = [
            1.65 + 1.50 * sin(t), ...
            1.65 + 1.50 * cos(t)
        ];
        reply = client.sequenceSet(points);
        wgs.printReply("SEQUENCE_SET 255 points", reply);
    end

    disp("Demo finished.");
end
