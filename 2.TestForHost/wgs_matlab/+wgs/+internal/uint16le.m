function bytes = uint16le(value)
    bytes = typecast(uint16(value), "uint8");
    if ~wgs.internal.isLittleEndian()
        bytes = fliplr(bytes);
    end
    bytes = reshape(bytes, 1, []);
end
