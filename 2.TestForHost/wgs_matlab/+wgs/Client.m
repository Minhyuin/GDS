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
            obj.sendCommandNoReply(obj.buildPointSetPayload(channel, voltage));
        end

        function reply = point2Set(obj, ch1Voltage, ch2Voltage)
            reply = obj.sendCommand(obj.buildPoint2SetPayload(ch1Voltage, ch2Voltage));
        end

        function point2SetNoReply(obj, ch1Voltage, ch2Voltage)
            obj.sendCommandNoReply(obj.buildPoint2SetPayload(ch1Voltage, ch2Voltage));
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
            if ~reply.isAck
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

        function payload = buildPointSetPayload(~, channel, voltage)
            C = wgs.internal.constants();
            payload = [
                uint8(C.CMD_POINT_SET), ...
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
