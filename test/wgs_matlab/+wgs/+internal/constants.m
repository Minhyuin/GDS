function C = constants()
    C = struct();
    C.SOF = hex2dec("A5");

    C.FRAME_COMMAND = hex2dec("01");
    C.FRAME_ACK = hex2dec("02");
    C.FRAME_NACK = hex2dec("03");

    C.CMD_PING = hex2dec("00");
    C.CMD_POINT_SET = hex2dec("10");
    C.CMD_SEQUENCE_SET = hex2dec("20");

    C.SEQUENCE_SET_HEADER_SIZE = 3;
    C.SEQUENCE_POINT_SIZE = 8;
    C.MAX_SEQUENCE_POINTS = 255;
end
