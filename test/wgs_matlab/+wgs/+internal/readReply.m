function reply = readReply(dev, verbose)
    if nargin < 2
        verbose = false;
    end

    C = wgs.internal.constants();

    [sof, scanned] = wgs.internal.readUntilSof(dev, C.SOF);
    if isempty(sof)
        error("WGS:ReplyTimeout", ...
              "Timed out waiting for WGS reply SOF. RX before timeout: %s", ...
              wgs.internal.bytesToHex(scanned));
    end

    header = wgs.internal.readExact(dev, 3);
    frameType = header(1);
    payloadLength = double(typecast(uint8(header(2:3)), "uint16"));
    if ~wgs.internal.isLittleEndian()
        payloadLength = double(swapbytes(uint16(payloadLength)));
    end

    payload = wgs.internal.readExact(dev, payloadLength);

    if payloadLength ~= 2
        error("WGS:BadReplyLength", ...
              "Expected 2-byte ACK/NACK payload, got %d bytes.", ...
              payloadLength);
    end

    reply = struct();
    reply.frameType = frameType;
    reply.status = payload(1);
    reply.detail = payload(2);
    reply.isAck = frameType == C.FRAME_ACK;
    reply.isNack = frameType == C.FRAME_NACK;
    reply.statusName = wgs.internal.ackStatusName(payload(1));
    reply.detailName = wgs.internal.errorCodeName(payload(2));

    if verbose
        fprintf("RX: %s\n", ...
                wgs.internal.bytesToHex([uint8(C.SOF), header, payload]));
    end
end
