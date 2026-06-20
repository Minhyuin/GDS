function client = connect(port, varargin)
%CONNECT Open a WGS UART client.
%
% Example:
%   client = wgs.connect("COM43");
%   cleanupObj = onCleanup(@() client.close());

    if nargin < 1
        port = "COM43";
    end

    client = wgs.Client(port, varargin{:});
end
