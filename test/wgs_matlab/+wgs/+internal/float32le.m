function bytes = float32le(value)
    bytes = typecast(single(value), "uint8");
    if ~wgs.internal.isLittleEndian()
        bytes = fliplr(bytes);
    end
    bytes = reshape(bytes, 1, []);
end
