function name = errorCodeName(code)
    switch code
        case hex2dec("00")
            name = "NONE";
        case hex2dec("01")
            name = "CRC_ERROR";
        case hex2dec("02")
            name = "BAD_LENGTH";
        case hex2dec("03")
            name = "UNSUPPORTED_COMMAND";
        case hex2dec("04")
            name = "INVALID_STATE";
        case hex2dec("05")
            name = "OUT_OF_RANGE";
        case hex2dec("06")
            name = "BUFFER_FULL";
        case hex2dec("07")
            name = "BAD_OFFSET";
        case hex2dec("08")
            name = "WAVE_CRC_MISMATCH";
        case hex2dec("09")
            name = "DMA_BUSY";
        case hex2dec("0A")
            name = "UNDERRUN";
        case hex2dec("0B")
            name = "TRANSPORT_BUSY";
        otherwise
            name = sprintf("UNKNOWN_0x%02X", code);
    end
end
