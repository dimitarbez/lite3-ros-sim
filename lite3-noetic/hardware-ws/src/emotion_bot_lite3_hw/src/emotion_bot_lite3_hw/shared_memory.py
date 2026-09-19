"""Versioned sequence-lock shared-memory record for high-rate telemetry."""

from __future__ import annotations

import mmap
import os
import struct
from typing import Optional, Tuple

MAGIC = b"EBL3"
VERSION = 1
HEADER = struct.Struct("<4sIQQI")
CAPACITY = 4096


class SharedTelemetry:
    def __init__(self, path: str, create: bool = False):
        flags = os.O_RDWR | (os.O_CREAT if create else 0)
        self.fd = os.open(path, flags, 0o640)
        if create:
            os.ftruncate(self.fd, CAPACITY)
        if os.fstat(self.fd).st_size != CAPACITY:
            os.close(self.fd)
            raise ValueError("unexpected shared telemetry size")
        self.mapping = mmap.mmap(self.fd, CAPACITY)
        if create and self.mapping[:4] != MAGIC:
            self.mapping[: HEADER.size] = HEADER.pack(MAGIC, VERSION, 0, 0, 0)

    def close(self):
        self.mapping.close()
        os.close(self.fd)

    def write(self, receive_ns: int, payload: bytes):
        if len(payload) > CAPACITY - HEADER.size:
            raise ValueError("telemetry payload too large")
        _, _, sequence, _, _ = HEADER.unpack(self.mapping[: HEADER.size])
        odd = sequence + 1 if sequence % 2 == 0 else sequence + 2
        self.mapping[: HEADER.size] = HEADER.pack(MAGIC, VERSION, odd, receive_ns, len(payload))
        self.mapping[HEADER.size : HEADER.size + len(payload)] = payload
        self.mapping[: HEADER.size] = HEADER.pack(MAGIC, VERSION, odd + 1, receive_ns, len(payload))

    def read(self, retries: int = 4) -> Optional[Tuple[int, int, bytes]]:
        for _ in range(retries):
            first = HEADER.unpack(self.mapping[: HEADER.size])
            magic, version, sequence, receive_ns, length = first
            if magic != MAGIC or version != VERSION or sequence % 2 or length > CAPACITY - HEADER.size:
                continue
            payload = bytes(self.mapping[HEADER.size : HEADER.size + length])
            second = HEADER.unpack(self.mapping[: HEADER.size])
            if first == second:
                return sequence, receive_ns, payload
        return None
