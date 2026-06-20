function name = ackStatusName(code)
    switch code
        case hex2dec("00")
            name = "ACCEPTED";
        case hex2dec("01")
            name = "DONE";
        case hex2dec("02")
            name = "BUSY";
        case hex2dec("80")
            name = "ERROR";
        otherwise
            name = sprintf("UNKNOWN_0x%02X", code);
    end
end
