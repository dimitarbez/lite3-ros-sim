"""Robot-side validation for loopback NDJSON emotion frames."""

from __future__ import annotations

import json
import math

MAX_FRAME_BYTES = 2048
EMOTIONS = {"neutral", "joy", "sadness", "anger", "fear", "surprise", "disgust", "curiosity", "affection"}


class TransportError(ValueError):
    pass


def validate_payload(state):
    if not isinstance(state, dict) or state.get("schema_version") != "1.1":
        raise TransportError("unsupported emotion-state schema")
    if state.get("emotion") not in EMOTIONS:
        raise TransportError("unknown emotion")
    for field, low, high in (("valence", -1.0, 1.0), ("arousal", 0.0, 1.0)):
        value = state.get(field)
        if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value):
            raise TransportError("invalid %s" % field)
        if not low <= value <= high:
            raise TransportError("out-of-range %s" % field)
    if not isinstance(state.get("sequence"), int) or isinstance(state["sequence"], bool) or state["sequence"] < 0:
        raise TransportError("invalid emotion sequence")
    if not isinstance(state.get("stamp"), dict):
        raise TransportError("invalid stamp")
    for field in ("backend", "source", "turn_id"):
        if not isinstance(state.get(field), str) or not state[field]:
            raise TransportError("invalid %s" % field)
    return state


def decode_frame(frame: bytes):
    if not frame or len(frame) > MAX_FRAME_BYTES or not frame.endswith(b"\n") or b"\n" in frame[:-1]:
        raise TransportError("invalid frame")
    try:
        envelope = json.loads(frame[:-1].decode("utf-8"))
    except (UnicodeDecodeError, ValueError) as exc:
        raise TransportError("malformed frame") from exc
    if not isinstance(envelope, dict) or set(envelope) != {"schema_version", "session_id", "sequence", "payload"}:
        raise TransportError("invalid envelope fields")
    if envelope["schema_version"] != "1.0" or not isinstance(envelope["session_id"], str) or not envelope["session_id"]:
        raise TransportError("invalid envelope identity")
    if not isinstance(envelope["sequence"], int) or isinstance(envelope["sequence"], bool) or envelope["sequence"] < 1:
        raise TransportError("invalid transport sequence")
    validate_payload(envelope["payload"])
    return envelope


class SequenceGate:
    def __init__(self):
        self.session_id = None
        self.sequence = 0

    def accept(self, envelope):
        if envelope["session_id"] != self.session_id:
            self.session_id = envelope["session_id"]
            self.sequence = envelope["sequence"]
            return True
        if envelope["sequence"] <= self.sequence:
            return False
        self.sequence = envelope["sequence"]
        return True
