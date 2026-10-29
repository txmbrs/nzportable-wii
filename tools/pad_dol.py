"""Include bytes read by 32-byte-aligned DOL loaders without changing sections."""
import struct
import sys
from pathlib import Path


def pad_dol(path):
    data = Path(path).read_bytes()
    header = struct.unpack(">64I", data[:256])
    required = len(data)
    for index in range(18):
        offset, size = header[index], header[36 + index]
        if not size:
            continue
        if offset < 256 or offset + size > len(data):
            raise ValueError(f"Invalid DOL section {index}")
        required = max(required, offset + ((size + 31) & ~31))
    padding = required - len(data)
    if padding:
        Path(path).write_bytes(data + bytes(padding))
    print(f"{path}: appended {padding} loader padding bytes")


if __name__ == "__main__":
    for argument in sys.argv[1:]:
        pad_dol(argument)
