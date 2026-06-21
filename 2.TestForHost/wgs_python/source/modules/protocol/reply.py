from dataclasses import dataclass

from .constants import AckStatus, ErrorCode, FrameType


@dataclass(frozen=True)
class Reply:
    frame_type: FrameType
    status: AckStatus
    detail: ErrorCode
    raw: bytes

    @property
    def is_ack(self):
        return self.frame_type == FrameType.ACK

    @property
    def is_nack(self):
        return self.frame_type == FrameType.NACK

    @property
    def ok(self):
        return self.is_ack and self.status == AckStatus.DONE and self.detail == ErrorCode.NONE

    def summary(self):
        kind = "ACK" if self.is_ack else "NACK" if self.is_nack else self.frame_type.name
        return f"{kind} status={self.status.name} detail={self.detail.name}"


def parse_reply(frame_type, payload, raw=b""):
    frame_type = FrameType(frame_type)
    if len(payload) != 2:
        raise ValueError(f"Reply payload length must be 2, got {len(payload)}")
    return Reply(
        frame_type=frame_type,
        status=AckStatus(payload[0]),
        detail=ErrorCode(payload[1]),
        raw=bytes(raw),
    )
