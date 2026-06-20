function data = readExact(dev, count)
    data = zeros(1, count, "uint8");
    got = 0;
    deadline = tic;

    while got < count
        if toc(deadline) >= dev.Timeout
            error("WGS:ReadTimeout", ...
                  "Timed out reading reply: got %d of %d bytes.", ...
                  got, count);
        end

        available = dev.NumBytesAvailable;
        if available < 1
            pause(0.001);
            continue;
        end

        chunkCount = min(count - got, available);
        chunk = read(dev, chunkCount, "uint8");
        data(got+1:got+chunkCount) = chunk;
        got = got + chunkCount;
    end
end
