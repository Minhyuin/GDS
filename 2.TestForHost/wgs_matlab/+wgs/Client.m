classdef Client < handle
    %CLIENT High-level MATLAB client for the WGS UART protocol.

    properties
        Port = "COM43"
        BaudRate = 921600
        Timeout = 5.0
        OpenStabilizeSeconds = 2.0
        MaxAttempts = 3
        Verbose = true
        SafeDtr = false
        SafeRts = false
    end

    properties (Access = private)
        Device = []
    end

    methods
        function obj = Client(port, varargin)
            if nargin < 1 || strlength(string(port)) == 0
                port = "COM43";
            end

            obj.Port = string(port);
            obj.applyOptions(varargin{:});
            obj.open();
        end

        function delete(obj)
            obj.close();
        end

        function close(obj)
            if ~isempty(obj.Device) && isvalid(obj.Device)
                obj.configureControlLines(false, false);
                flush(obj.Device);
            end
            obj.Device = [];
        end

        function flushInput(obj)
            obj.ensureOpen();
            flush(obj.Device, "input");
        end

        function reply = ping(obj)
            C = wgs.internal.constants();
            reply = obj.sendCommand(uint8(C.CMD_PING));
        end

        function reply = sync(obj)
            C = wgs.internal.constants();
            reply = obj.sendCommand(uint8(C.CMD_SYNC));
        end

        function reply = pointSet(obj, channel, voltage)
            reply = obj.sendCommand(obj.buildPointSetPayload(channel, voltage));
        end

        function pointSetNoReply(obj, channel, voltage)
            obj.sendCommandNoReply(obj.buildPointSetNoAckPayload(channel, voltage));
        end

        function pointSetNoAck(obj, channel, voltage)
            obj.pointSetNoReply(channel, voltage);
        end

        function reply = point2Set(obj, ch1Voltage, ch2Voltage)
            reply = obj.sendCommand(obj.buildPoint2SetPayload(ch1Voltage, ch2Voltage));
        end

        function point2SetNoReply(obj, ch1Voltage, ch2Voltage)
            obj.sendCommandNoReply(obj.buildPoint2SetNoAckPayload(ch1Voltage, ch2Voltage));
        end

        function point2SetNoAck(obj, ch1Voltage, ch2Voltage)
            obj.point2SetNoReply(ch1Voltage, ch2Voltage);
        end

        function stats = point2SetNoReplyBatch(obj, ch1Voltages, ch2Voltages, varargin)
            options = obj.parsePointBatchOptions(varargin{:});
            frames = wgs.internal.buildPoint2NoAckFrames(ch1Voltages, ch2Voltages);
            pointCount = numel(reshape(ch1Voltages, 1, []));
            stats = obj.writeFrameBatches(frames, 13, pointCount, options);
        end

        function stats = point2SetNoAckBatch(obj, ch1Voltages, ch2Voltages, varargin)
            stats = obj.point2SetNoReplyBatch(ch1Voltages, ch2Voltages, varargin{:});
        end

        function reply = pointFifoClear(obj, channel)
            C = wgs.internal.constants();
            payload = uint8([C.CMD_POINT_FIFO_CLEAR, wgs.internal.channelByte(channel)]);
            reply = obj.sendCommand(payload);
        end

        function reply = pointFifoAppend(obj, channel, voltages)
            payload = wgs.internal.buildPointFifoAppendPayload(channel, voltages);
            reply = obj.sendCommand(payload);
        end

        function reply = pointFifoStart(obj, channel)
            C = wgs.internal.constants();
            payload = uint8([C.CMD_POINT_FIFO_START, wgs.internal.channelByte(channel)]);
            reply = obj.sendCommand(payload);
        end

        function reply = pointFifoStop(obj, channel)
            C = wgs.internal.constants();
            payload = uint8([C.CMD_POINT_FIFO_STOP, wgs.internal.channelByte(channel)]);
            reply = obj.sendCommand(payload);
        end

        function reply = point2FifoClear(obj)
            C = wgs.internal.constants();
            reply = obj.sendCommand(uint8(C.CMD_POINT2_FIFO_CLEAR));
        end

        function reply = point2FifoAppend(obj, ch1Voltages, ch2Voltages)
            payload = wgs.internal.buildPoint2FifoAppendPayload(ch1Voltages, ch2Voltages);
            reply = obj.sendCommand(payload);
        end

        function reply = point2FifoStart(obj)
            C = wgs.internal.constants();
            reply = obj.sendCommand(uint8(C.CMD_POINT2_FIFO_START));
        end

        function reply = point2FifoStop(obj)
            C = wgs.internal.constants();
            reply = obj.sendCommand(uint8(C.CMD_POINT2_FIFO_STOP));
        end

        function stats = point2FifoStream(obj, ch1Voltages, ch2Voltages, varargin)
            ch1Voltages = reshape(ch1Voltages, 1, []);
            ch2Voltages = reshape(ch2Voltages, 1, []);
            if numel(ch1Voltages) ~= numel(ch2Voltages)
                error("WGS:BadPoint2FifoStream", ...
                      "POINT2 FIFO stream vectors must have the same length.");
            end

            options = obj.parsePoint2FifoStreamOptions(varargin{:});
            totalCount = numel(ch1Voltages);
            stats = struct( ...
                "sentCount", 0, ...
                "elapsedSeconds", 0.0, ...
                "actualAppendRateHz", 0.0);

            clearReply = obj.point2FifoClear();
            if ~obj.isDoneReply(clearReply)
                error("WGS:Point2FifoClearFailed", ...
                      "POINT2_FIFO_CLEAR failed: status=%s detail=%s.", ...
                      clearReply.statusName, clearReply.detailName);
            end

            startTime = tic;
            nextIndex = 1;
            preloadTarget = min(totalCount, options.PreloadPointCount);
            [nextIndex, stats] = obj.appendPoint2FifoRangeWithRetry( ...
                ch1Voltages, ch2Voltages, nextIndex, preloadTarget, ...
                totalCount, options, startTime, stats);

            startReply = obj.point2FifoStart();
            if ~obj.isDoneReply(startReply)
                error("WGS:Point2FifoStartFailed", ...
                      "POINT2_FIFO_START failed: status=%s detail=%s.", ...
                      startReply.statusName, startReply.detailName);
            end

            [nextIndex, stats] = obj.appendPoint2FifoRangeWithRetry( ...
                ch1Voltages, ch2Voltages, nextIndex, totalCount, ...
                totalCount, options, startTime, stats);

            if options.StopAfterSend
                stopReply = obj.point2FifoStop();
                if ~obj.isDoneReply(stopReply)
                    error("WGS:Point2FifoStopFailed", ...
                          "POINT2_FIFO_STOP failed: status=%s detail=%s.", ...
                          stopReply.statusName, stopReply.detailName);
                end
            end

            stats.sentCount = nextIndex - 1;
            stats.elapsedSeconds = toc(startTime);
            stats.actualAppendRateHz = stats.sentCount / max(stats.elapsedSeconds, eps);
        end

        function reply = pointPulseWidthSet(obj, pulseWidthUs)
            reply = obj.sendCommand(obj.buildPointPulseWidthSetPayload(pulseWidthUs));
        end

        function reply = sequenceSet(obj, channel, samples, frequencyHz)
            C = wgs.internal.constants();

            samples = reshape(samples, 1, []);
            sampleCount = numel(samples);
            if sampleCount < 1 || sampleCount > C.MAX_SEQUENCE_POINTS
                error("WGS:BadSampleCount", ...
                      "SEQUENCE_SET sample count must be in 1..%d.", ...
                      C.MAX_SEQUENCE_POINTS);
            end

            if nargin < 4
                error("WGS:MissingFrequency", ...
                      "sequenceSet requires frequencyHz so the client can configure waveform length first.");
            end

            reply = obj.waveformConfig(channel, sampleCount, frequencyHz);
            if ~obj.isDoneReply(reply)
                return;
            end

            reply = obj.sendSequencePayload(channel, samples);
        end

        function reply = waveformConfig(obj, channel, lengthSa, frequencyHz)
            payload = obj.buildWaveformConfigPayload(channel, lengthSa, frequencyHz);
            reply = obj.sendCommand(payload);
        end

        function reply = setSampleRate(obj, channel, sampleRateHz)
            C = wgs.internal.constants();
            payload = [
                uint8(C.CMD_TIMER_SET), ...
                wgs.internal.channelByte(channel), ...
                wgs.internal.float32le(sampleRateHz)
            ];
            reply = obj.sendCommand(payload);
        end

        function reply = setWaveFrequency(obj, channel, waveformFrequencyHz)
            C = wgs.internal.constants();
            reply = obj.setSampleRate(channel, ...
                                      waveformFrequencyHz * ...
                                      C.PERIOD_SAMPLE_COUNT);
        end
    end

    methods (Access = private)
        function ok = isDoneReply(~, reply)
            C = wgs.internal.constants();
            ok = reply.isAck && ...
                 reply.status == C.ACK_DONE && ...
                 reply.detail == C.ERROR_NONE;
        end

        function ok = isBusyReply(~, reply)
            C = wgs.internal.constants();
            ok = reply.isAck && ...
                 reply.status == C.ACK_BUSY && ...
                 reply.detail == C.ERROR_NONE;
        end

        function reply = sendSequencePayload(obj, channel, samples)
            C = wgs.internal.constants();
            sampleCount = numel(samples);
            payload = zeros(1, C.SEQUENCE_SET_HEADER_SIZE + sampleCount * 4, ...
                            "uint8");
            payload(1) = uint8(C.CMD_SEQUENCE_SET);
            payload(2) = wgs.internal.channelByte(channel);
            payload(3:4) = wgs.internal.uint16le(sampleCount);

            offset = C.SEQUENCE_SET_HEADER_SIZE + 1;
            for i = 1:sampleCount
                payload(offset:offset+3) = wgs.internal.float32le(samples(i));
                offset = offset + 4;
            end

            reply = obj.sendCommand(payload);
        end

        function open(obj)
            obj.Device = serialport(obj.Port, obj.BaudRate, ...
                "Timeout", obj.Timeout, ...
                "FlowControl", "none");

            obj.configureControlLines(obj.SafeDtr, obj.SafeRts);
            pause(obj.OpenStabilizeSeconds);
            flush(obj.Device);
        end

        function configureControlLines(obj, dtrState, rtsState)
            try
                setDTR(obj.Device, dtrState);
                setRTS(obj.Device, rtsState);
            catch err
                warning("WGS:ControlLinesUnsupported", ...
                        "Could not configure DTR/RTS: %s", err.message);
            end
        end

        function reply = sendCommand(obj, payload)
            obj.ensureOpen();

            C = wgs.internal.constants();
            frame = wgs.internal.buildFrame(C.FRAME_COMMAND, payload);

            for attempt = 1:obj.MaxAttempts
                flush(obj.Device, "input");
                if obj.Verbose
                    fprintf("TX[%d]: %s\n", attempt, ...
                            wgs.internal.bytesToHex(frame));
                end

                write(obj.Device, frame, "uint8");
                pause(0.001);

                try
                    reply = wgs.internal.readReply(obj.Device, obj.Verbose);
                    return;
                catch err
                    if attempt >= obj.MaxAttempts || ...
                            ~strcmp(err.identifier, "WGS:ReplyTimeout")
                        rethrow(err);
                    end

                    if obj.Verbose
                        fprintf("No reply, retrying command...\n");
                    end
                    pause(0.03);
                end
            end
        end

        function sendCommandNoReply(obj, payload)
            obj.ensureOpen();

            C = wgs.internal.constants();
            frame = wgs.internal.buildFrame(C.FRAME_COMMAND, payload);

            if obj.Verbose
                fprintf("TX: %s\n", wgs.internal.bytesToHex(frame));
            end

            write(obj.Device, frame, "uint8");
        end

        function stats = writeFrameBatches(obj, frames, frameLength, pointCount, options)
            obj.ensureOpen();

            stats = struct( ...
                "sentCount", 0, ...
                "elapsedSeconds", 0.0, ...
                "actualRateHz", 0.0);

            if pointCount == 0
                return;
            end

            if numel(frames) ~= pointCount * frameLength
                error("WGS:BadFrameBatch", ...
                      "Frame batch length does not match frame and point counts.");
            end

            startTime = tic;
            for batchStart = 1:options.BatchPointCount:pointCount
                batchEnd = min(batchStart + options.BatchPointCount - 1, pointCount);
                byteStart = (batchStart - 1) * frameLength + 1;
                byteEnd = batchEnd * frameLength;

                if obj.Verbose
                    fprintf("TX batch: points %d..%d (%d bytes)\n", ...
                            batchStart, batchEnd, byteEnd - byteStart + 1);
                end

                write(obj.Device, frames(byteStart:byteEnd), "uint8");

                stats.sentCount = batchEnd;
                if ~isempty(options.ProgressFcn)
                    options.ProgressFcn(stats.sentCount, pointCount);
                end

                if ~isempty(options.PointRateHz)
                    targetElapsed = stats.sentCount / options.PointRateHz;
                    while toc(startTime) < targetElapsed
                        pause(0.001);
                    end
                end
            end

            stats.elapsedSeconds = toc(startTime);
            stats.actualRateHz = stats.sentCount / max(stats.elapsedSeconds, eps);
        end

        function payload = buildPointSetPayload(~, channel, voltage)
            C = wgs.internal.constants();
            payload = [
                uint8(C.CMD_POINT_SET), ...
                wgs.internal.channelByte(channel), ...
                wgs.internal.float32le(voltage)
            ];
        end

        function payload = buildPointSetNoAckPayload(~, channel, voltage)
            C = wgs.internal.constants();
            payload = [
                uint8(C.CMD_POINT_SET_NO_ACK), ...
                wgs.internal.channelByte(channel), ...
                wgs.internal.float32le(voltage)
            ];
        end

        function payload = buildPoint2SetPayload(~, ch1Voltage, ch2Voltage)
            C = wgs.internal.constants();
            payload = [
                uint8(C.CMD_POINT2_SET), ...
                wgs.internal.float32le(ch1Voltage), ...
                wgs.internal.float32le(ch2Voltage)
            ];
        end

        function payload = buildPoint2SetNoAckPayload(~, ch1Voltage, ch2Voltage)
            C = wgs.internal.constants();
            payload = [
                uint8(C.CMD_POINT2_SET_NO_ACK), ...
                wgs.internal.float32le(ch1Voltage), ...
                wgs.internal.float32le(ch2Voltage)
            ];
        end

        function payload = buildPointPulseWidthSetPayload(~, pulseWidthUs)
            C = wgs.internal.constants();
            if ~isfinite(pulseWidthUs) || ...
                    pulseWidthUs < C.POINT_PULSE_WIDTH_MIN_US || ...
                    pulseWidthUs > C.POINT_PULSE_WIDTH_MAX_US
                error("WGS:BadPointPulseWidth", ...
                      "POINT_PULSE_WIDTH_SET pulse width must be %.0f..%.0f us.", ...
                      C.POINT_PULSE_WIDTH_MIN_US, C.POINT_PULSE_WIDTH_MAX_US);
            end
            payload = [
                uint8(C.CMD_POINT_PULSE_WIDTH_SET), ...
                wgs.internal.float32le(pulseWidthUs)
            ];
        end

        function payload = buildWaveformConfigPayload(~, channel, lengthSa, frequencyHz)
            C = wgs.internal.constants();
            lengthSa = double(lengthSa);
            frequencyHz = double(frequencyHz);
            if lengthSa ~= round(lengthSa) || ...
                    lengthSa < 1 || lengthSa > C.MAX_WAVEFORM_LENGTH_SA
                error("WGS:BadWaveformLength", ...
                      "Waveform length must be an integer in 1..%d Sa.", ...
                      C.MAX_WAVEFORM_LENGTH_SA);
            end
            if ~isfinite(frequencyHz) || frequencyHz <= 0
                error("WGS:BadWaveformFrequency", ...
                      "Waveform frequency must be a positive finite value.");
            end
            sampleRateHz = lengthSa * frequencyHz;
            if sampleRateHz > C.MAX_SAMPLE_RATE_HZ
                error("WGS:BadSampleRate", ...
                      "Waveform sample rate %.3f Sa/s exceeds %.3f Sa/s.", ...
                      sampleRateHz, C.MAX_SAMPLE_RATE_HZ);
            end
            payload = [
                uint8(C.CMD_WAVEFORM_CONFIG), ...
                wgs.internal.channelByte(channel), ...
                wgs.internal.uint16le(uint16(lengthSa)), ...
                wgs.internal.float32le(frequencyHz)
            ];
        end

        function ensureOpen(obj)
            if isempty(obj.Device) || ~isvalid(obj.Device)
                error("WGS:SerialClosed", "WGS serial client is closed.");
            end
        end

        function options = parsePointBatchOptions(~, varargin)
            if mod(numel(varargin), 2) ~= 0
                error("WGS:BadOptions", ...
                      "Options must be name-value pairs.");
            end

            options = struct( ...
                "BatchPointCount", 500, ...
                "PointRateHz", [], ...
                "ProgressFcn", []);

            for k = 1:2:numel(varargin)
                name = lower(string(varargin{k}));
                value = varargin{k + 1};

                switch name
                    case "batchpointcount"
                        value = double(value);
                        if ~isfinite(value) || value < 1 || value ~= round(value)
                            error("WGS:BadBatchPointCount", ...
                                  "BatchPointCount must be a positive integer.");
                        end
                        options.BatchPointCount = value;
                    case "pointratehz"
                        if isempty(value)
                            options.PointRateHz = [];
                        else
                            value = double(value);
                            if ~isfinite(value) || value <= 0
                                error("WGS:BadPointRate", ...
                                      "PointRateHz must be a positive finite value.");
                            end
                            options.PointRateHz = value;
                        end
                    case "progressfcn"
                        if isempty(value)
                            options.ProgressFcn = [];
                        elseif isa(value, 'function_handle')
                            options.ProgressFcn = value;
                        else
                            error("WGS:BadProgressFcn", ...
                                  "ProgressFcn must be a function handle or empty.");
                        end
                    otherwise
                        error("WGS:UnknownOption", ...
                              "Unknown batch option: %s", name);
                end
            end
        end

        function options = parsePoint2FifoStreamOptions(obj, varargin)
            options = struct( ...
                "AppendPointCount", 500, ...
                "PreloadPointCount", 500, ...
                "RetryPauseSeconds", 0.002, ...
                "AppendTimeoutSeconds", 10.0, ...
                "StopAfterSend", false, ...
                "ProgressFcn", []);

            if mod(numel(varargin), 2) ~= 0
                error("WGS:BadOptions", ...
                      "Options must be name-value pairs.");
            end

            for k = 1:2:numel(varargin)
                name = lower(string(varargin{k}));
                value = varargin{k + 1};

                switch name
                    case "appendpointcount"
                        value = double(value);
                        if ~isfinite(value) || value < 1 || value ~= round(value) || value > 500
                            error("WGS:BadAppendPointCount", ...
                                  "AppendPointCount must be an integer in 1..500.");
                        end
                        options.AppendPointCount = value;
                    case "preloadpointcount"
                        value = double(value);
                        if ~isfinite(value) || value < 0 || value ~= round(value)
                            error("WGS:BadPreloadPointCount", ...
                                  "PreloadPointCount must be a non-negative integer.");
                        end
                        options.PreloadPointCount = value;
                    case "retrypauseseconds"
                        value = double(value);
                        if ~isfinite(value) || value < 0
                            error("WGS:BadRetryPause", ...
                                  "RetryPauseSeconds must be a non-negative finite value.");
                        end
                        options.RetryPauseSeconds = value;
                    case "appendtimeoutseconds"
                        value = double(value);
                        if ~isfinite(value) || value <= 0
                            error("WGS:BadAppendTimeout", ...
                                  "AppendTimeoutSeconds must be a positive finite value.");
                        end
                        options.AppendTimeoutSeconds = value;
                    case "stopaftersend"
                        options.StopAfterSend = logical(value);
                    case "progressfcn"
                        if isempty(value)
                            options.ProgressFcn = [];
                        elseif isa(value, 'function_handle')
                            options.ProgressFcn = value;
                        else
                            error("WGS:BadProgressFcn", ...
                                  "ProgressFcn must be a function handle or empty.");
                        end
                    otherwise
                        error("WGS:UnknownOption", ...
                              "Unknown POINT2 FIFO stream option: %s", name);
                end
            end
        end

        function [nextIndex, stats] = appendPoint2FifoRangeWithRetry( ...
                obj, ch1Voltages, ch2Voltages, nextIndex, endIndex, ...
                totalCount, options, startTime, stats)
            while nextIndex <= endIndex
                chunkEnd = min(nextIndex + options.AppendPointCount - 1, endIndex);
                retryStart = tic;
                while true
                    reply = obj.point2FifoAppend( ...
                        ch1Voltages(nextIndex:chunkEnd), ...
                        ch2Voltages(nextIndex:chunkEnd));

                    if obj.isDoneReply(reply)
                        nextIndex = chunkEnd + 1;
                        stats.sentCount = nextIndex - 1;
                        stats.elapsedSeconds = toc(startTime);
                        stats.actualAppendRateHz = ...
                            stats.sentCount / max(stats.elapsedSeconds, eps);
                        if ~isempty(options.ProgressFcn)
                            options.ProgressFcn(stats.sentCount, totalCount);
                        end
                        break;
                    end

                    if ~obj.isBusyReply(reply)
                        error("WGS:Point2FifoAppendFailed", ...
                              "POINT2_FIFO_APPEND failed: status=%s detail=%s.", ...
                              reply.statusName, reply.detailName);
                    end
                    if toc(retryStart) > options.AppendTimeoutSeconds
                        error("WGS:Point2FifoAppendTimeout", ...
                              "Timed out waiting for POINT2 FIFO space.");
                    end
                    pause(options.RetryPauseSeconds);
                end
            end
        end

        function applyOptions(obj, varargin)
            if mod(numel(varargin), 2) ~= 0
                error("WGS:BadOptions", ...
                      "Options must be name-value pairs.");
            end

            for k = 1:2:numel(varargin)
                name = lower(string(varargin{k}));
                value = varargin{k + 1};

                switch name
                    case "baudrate"
                        obj.BaudRate = value;
                    case "timeout"
                        obj.Timeout = value;
                    case "openstabilizeseconds"
                        obj.OpenStabilizeSeconds = value;
                    case "maxattempts"
                        obj.MaxAttempts = value;
                    case "verbose"
                        obj.Verbose = logical(value);
                    case "safedtr"
                        obj.SafeDtr = logical(value);
                    case "saferts"
                        obj.SafeRts = logical(value);
                    otherwise
                        error("WGS:UnknownOption", ...
                              "Unknown WGS client option: %s", name);
                end
            end
        end
    end
end
