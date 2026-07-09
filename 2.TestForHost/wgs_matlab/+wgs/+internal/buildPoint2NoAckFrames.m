function frames = buildPoint2NoAckFrames(ch1Voltages, ch2Voltages)
    ch1Voltages = reshape(ch1Voltages, 1, []);
    ch2Voltages = reshape(ch2Voltages, 1, []);

    pointCount = numel(ch1Voltages);
    if pointCount ~= numel(ch2Voltages)
        error("WGS:BadPoint2Batch", ...
              "POINT2_SET_NO_ACK batch vectors must have the same length.");
    end

    C = wgs.internal.constants();
    frameLength = 13;
    frames = zeros(1, pointCount * frameLength, "uint8");

    for i = 1:pointCount
        payload = [
            uint8(C.CMD_POINT2_SET_NO_ACK), ...
            wgs.internal.float32le(ch1Voltages(i)), ...
            wgs.internal.float32le(ch2Voltages(i))
        ];
        offset = (i - 1) * frameLength + 1;
        frames(offset:offset + frameLength - 1) = ...
            wgs.internal.buildFrame(C.FRAME_COMMAND, payload);
    end
end
