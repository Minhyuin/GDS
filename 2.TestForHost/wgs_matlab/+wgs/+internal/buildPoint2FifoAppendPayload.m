function payload = buildPoint2FifoAppendPayload(ch1Voltages, ch2Voltages)
    ch1Voltages = reshape(ch1Voltages, 1, []);
    ch2Voltages = reshape(ch2Voltages, 1, []);

    pointCount = numel(ch1Voltages);
    C = wgs.internal.constants();
    if pointCount ~= numel(ch2Voltages)
        error("WGS:BadPoint2FifoAppend", ...
              "POINT2_FIFO_APPEND vectors must have the same length.");
    end
    if pointCount < 1 || pointCount > C.POINT2_FIFO_APPEND_MAX_POINTS
        error("WGS:BadPoint2FifoAppend", ...
              "POINT2_FIFO_APPEND point count must be in 1..%d.", ...
              C.POINT2_FIFO_APPEND_MAX_POINTS);
    end

    payload = zeros(1, 3 + pointCount * 8, "uint8");
    payload(1) = uint8(C.CMD_POINT2_FIFO_APPEND);
    payload(2:3) = wgs.internal.uint16le(pointCount);

    offset = 4;
    for i = 1:pointCount
        payload(offset:offset+3) = wgs.internal.float32le(ch1Voltages(i));
        payload(offset+4:offset+7) = wgs.internal.float32le(ch2Voltages(i));
        offset = offset + 8;
    end
end
