function [byte, scanned] = readUntilSof(dev, sof)
    byte = [];
    scanned = zeros(1, 0, "uint8");
    deadline = tic;

    while toc(deadline) < dev.Timeout
        if dev.NumBytesAvailable < 1
            pause(0.001);
            continue;
        end

        candidate = read(dev, 1, "uint8");
        scanned(end+1) = candidate; %#ok<AGROW>
        if candidate == sof
            byte = candidate;
            return;
        end
    end
end
