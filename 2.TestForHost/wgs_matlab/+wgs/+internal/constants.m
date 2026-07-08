function C = constants()
    C = struct();
    C.SOF = hex2dec("A5");

    C.FRAME_COMMAND = hex2dec("01");
    C.FRAME_ACK = hex2dec("02");
    C.FRAME_NACK = hex2dec("03");

    C.CMD_PING = hex2dec("00");
    C.CMD_SYNC = hex2dec("01");
    C.CMD_POINT_SET = hex2dec("10");
    C.CMD_POINT2_SET = hex2dec("11");
    C.CMD_SEQUENCE_SET = hex2dec("20");
    C.CMD_WAVEFORM_CONFIG = hex2dec("30");
    C.CMD_TIMER_SET = hex2dec("40");
    C.CMD_POINT_PULSE_WIDTH_SET = hex2dec("50");

    C.CHANNEL_1 = hex2dec("01");
    C.CHANNEL_2 = hex2dec("02");

    C.PERIOD_SAMPLE_COUNT = 1000;
    C.MAX_WAVEFORM_LENGTH_SA = C.PERIOD_SAMPLE_COUNT;
    C.MAX_SAMPLE_RATE_HZ = 100000.0;
    C.POINT_PULSE_WIDTH_DEFAULT_US = 10.0;
    C.POINT_PULSE_WIDTH_MIN_US = 1.0;
    C.POINT_PULSE_WIDTH_MAX_US = 1000000.0;
    C.SEQUENCE_SET_HEADER_SIZE = 4;
    C.SEQUENCE_POINT_SIZE = 4;
    C.MAX_SEQUENCE_POINTS = C.PERIOD_SAMPLE_COUNT;
end
