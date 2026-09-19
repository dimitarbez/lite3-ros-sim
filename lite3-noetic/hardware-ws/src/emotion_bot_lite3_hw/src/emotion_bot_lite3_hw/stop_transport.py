"""Authenticated, replay-resistant transport for passive Retroid observations."""

from __future__ import annotations

import hashlib
import hmac
import json
from dataclasses import dataclass
from typing import Optional

SCHEMA_VERSION = "1.0"
MAX_FRAME_BYTES = 1024


class StopRelayError(ValueError):
    pass


@dataclass(frozen=True)
class StopRelayFrame:
    session_id: str
    sequence: int
    stop: bool
    axes_zero: bool
    observed_fresh: bool


def load_key(path: str) -> bytes:
    with open(path, "rb") as stream:
        value = stream.read().strip()
    if len(value) < 32:
        raise StopRelayError("STOP relay key must contain at least 32 bytes")
    return value


def _canonical(value: dict) -> bytes:
    return json.dumps(value, sort_keys=True, separators=(",", ":")).encode("utf-8")


def encode_frame(key: bytes, frame: StopRelayFrame) -> bytes:
    if not key or len(key) < 32:
        raise StopRelayError("invalid STOP relay key")
    if not frame.session_id or len(frame.session_id) > 128 or frame.sequence < 1:
        raise StopRelayError("invalid STOP relay session or sequence")
    body = {
        "schema_version": SCHEMA_VERSION,
        "session_id": frame.session_id,
        "sequence": frame.sequence,
        "stop": frame.stop,
        "axes_zero": frame.axes_zero,
        "observed_fresh": frame.observed_fresh,
    }
    body["mac"] = hmac.new(key, _canonical(body), hashlib.sha256).hexdigest()
    encoded = _canonical(body) + b"\n"
    if len(encoded) > MAX_FRAME_BYTES:
        raise StopRelayError("STOP relay frame is too large")
    return encoded


def decode_frame(key: bytes, encoded: bytes) -> StopRelayFrame:
    if not key or len(key) < 32:
        raise StopRelayError("invalid STOP relay key")
    if not encoded or len(encoded) > MAX_FRAME_BYTES or not encoded.endswith(b"\n"):
        raise StopRelayError("invalid STOP relay frame boundary")
    try:
        envelope = json.loads(encoded.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise StopRelayError("invalid STOP relay JSON") from exc
    expected_keys = {
        "schema_version", "session_id", "sequence", "stop", "axes_zero",
        "observed_fresh", "mac",
    }
    if not isinstance(envelope, dict) or set(envelope) != expected_keys:
        raise StopRelayError("invalid STOP relay fields")
    mac = envelope.pop("mac")
    expected_mac = hmac.new(key, _canonical(envelope), hashlib.sha256).hexdigest()
    if not isinstance(mac, str) or not hmac.compare_digest(mac, expected_mac):
        raise StopRelayError("invalid STOP relay MAC")
    if envelope["schema_version"] != SCHEMA_VERSION:
        raise StopRelayError("unsupported STOP relay schema")
    session_id = envelope["session_id"]
    sequence = envelope["sequence"]
    boolean_values = (envelope["stop"], envelope["axes_zero"], envelope["observed_fresh"])
    if not isinstance(session_id, str) or not session_id or len(session_id) > 128:
        raise StopRelayError("invalid STOP relay session")
    if isinstance(sequence, bool) or not isinstance(sequence, int) or sequence < 1:
        raise StopRelayError("invalid STOP relay sequence")
    if not all(isinstance(value, bool) for value in boolean_values):
        raise StopRelayError("invalid STOP relay boolean")
    return StopRelayFrame(
        session_id=session_id,
        sequence=sequence,
        stop=envelope["stop"],
        axes_zero=envelope["axes_zero"],
        observed_fresh=envelope["observed_fresh"],
    )


class StopRelaySequenceGate:
    """Reject replay and do not switch sessions while the current relay is live."""

    def __init__(self):
        self.session_id: Optional[str] = None
        self.sequence = 0

    def accept(self, frame: StopRelayFrame, current_session_stale: bool) -> bool:
        if self.session_id is None:
            self.session_id = frame.session_id
            self.sequence = frame.sequence
            return True
        if frame.session_id != self.session_id:
            if not current_session_stale:
                return False
            self.session_id = frame.session_id
            self.sequence = frame.sequence
            return True
        if frame.sequence <= self.sequence:
            return False
        self.sequence = frame.sequence
        return True
