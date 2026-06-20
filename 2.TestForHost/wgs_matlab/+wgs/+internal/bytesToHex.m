function text = bytesToHex(bytes)
    bytes = reshape(uint8(bytes), 1, []);
    if isempty(bytes)
        text = "<empty>";
        return;
    end

    text = strjoin(cellstr(dec2hex(bytes, 2)), " ");
end
