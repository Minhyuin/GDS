function payload = buildPointFifoAppendPayload(channel, voltages)
    voltages = reshape(voltages, 1, []);

    pointCount = numel(voltages);
    C = wgs.internal.constants();
    if pointCount < 1 || pointCount > C.POINT_FIFO_APPEND_MAX_POINTS
        error("WGS:BadPointFifoAppend", ...
              "POINT_FIFO_APPEND point count must be in 1..%d.", ...
              C.POINT_FIFO_APPEND_MAX_POINTS);
    end

    payload = zeros(1, 4 + pointCount * 4, "uint8");
    payload(1) = uint8(C.CMD_POINT_FIFO_APPEND);
    payload(2) = wgs.internal.channelByte(channel);
    payload(3:4) = wgs.internal.uint16le(pointCount);

    offset = 5;
    for i = 1:pointCount
        payload(offset:offset+3) = wgs.internal.float32le(voltages(i));
        offset = offset + 4;
    end
end
