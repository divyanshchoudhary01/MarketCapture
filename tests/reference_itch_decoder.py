#!/usr/bin/env python3
"""Independent minimal ITCH decoder used only as a golden-test oracle."""
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

def add(order_id: int) -> bytes:
    value = bytearray(36)
    value[0] = ord("A")
    value[5:11] = (123456).to_bytes(6, "big")
    value[11:19] = order_id.to_bytes(8, "big")
    value[19] = ord("B")
    value[20:24] = (100).to_bytes(4, "big")
    value[24:32] = b"AAPL    "
    value[32:36] = (1892500).to_bytes(4, "big")
    return bytes(value)

def delete(order_id: int) -> bytes:
    value = bytearray(19)
    value[0] = ord("D")
    value[5:11] = (123457).to_bytes(6, "big")
    value[11:19] = order_id.to_bytes(8, "big")
    return bytes(value)

def decode(message: bytes):
    kind = chr(message[0])
    expected = {"A": 36, "D": 19}[kind]
    assert len(message) == expected
    return kind, int.from_bytes(message[11:19], "big")

messages = [add(42), delete(42)]
assert [decode(message) for message in messages] == [("A", 42), ("D", 42)]
payload = b"".join(struct.pack(">H", len(message)) + message for message in messages)
with tempfile.TemporaryDirectory() as directory:
    fixture = Path(directory) / "golden.itch"
    fixture.write_bytes(payload)
    result = subprocess.run([sys.argv[1], str(fixture)], check=True,
                            text=True, capture_output=True)
    assert "messages=2" in result.stdout
    assert "order_ticks=2" in result.stdout
    assert "A=1" in result.stdout and "D=1" in result.stdout
print("Independent ITCH golden differential test passed")
