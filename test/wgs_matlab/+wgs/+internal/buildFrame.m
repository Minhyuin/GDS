function frame = buildFrame(frameType, payload)
    payload = uint8(payload);
    lengthBytes = wgs.internal.uint16le(numel(payload));
    C = wgs.internal.constants();

    frame = [
        uint8(C.SOF), ...
        uint8(frameType), ...
        lengthBytes, ...
        payload
    ];
end
