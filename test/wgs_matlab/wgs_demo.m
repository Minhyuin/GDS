clc;
clear;

client = wgs.connect("COM43");
cleanupObj = onCleanup(@() client.close());

reply = client.ping();
wgs.printReply("PING", reply);

reply = client.pointSet(1.65, 1.65);
wgs.printReply("POINT_SET", reply);

t = linspace(0, 2*pi, 128).';
points = [
    1.65 + 1.40 * sin(t), ...
    1.65 + 1.40 * cos(t)
];

reply = client.sequenceSet(points);
wgs.printReply("SEQUENCE_SET", reply);