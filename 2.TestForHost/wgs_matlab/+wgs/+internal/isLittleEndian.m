function tf = isLittleEndian()
    [~, ~, endian] = computer;
    tf = strcmp(endian, "L");
end
