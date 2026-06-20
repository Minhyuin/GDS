function printReply(label, reply)
%PRINTREPLY Print a compact ACK/NACK summary.

    if reply.isAck
        fprintf("%s -> ACK status=%s detail=%s\n", ...
                label, reply.statusName, reply.detailName);
    elseif reply.isNack
        fprintf("%s -> NACK status=%s detail=%s\n", ...
                label, reply.statusName, reply.detailName);
    else
        fprintf("%s -> frameType=0x%02X status=%s detail=%s\n", ...
                label, reply.frameType, reply.statusName, reply.detailName);
    end
end
