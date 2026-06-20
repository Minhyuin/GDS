function value = channelByte(channel)
    C = wgs.internal.constants();

    if isnumeric(channel)
        channelNumber = double(channel);
    else
        text = upper(string(channel));
        switch text
            case {"1", "CH1", "CHANNEL1", "X"}
                channelNumber = 1;
            case {"2", "CH2", "CHANNEL2", "Y"}
                channelNumber = 2;
            otherwise
                error("WGS:BadChannel", ...
                      "channel must be 1, 2, CH1, CH2, X, or Y.");
        end
    end

    switch channelNumber
        case 1
            value = uint8(C.CHANNEL_1);
        case 2
            value = uint8(C.CHANNEL_2);
        otherwise
            error("WGS:BadChannel", ...
                  "channel must be 1, 2, CH1, CH2, X, or Y.");
    end
end
