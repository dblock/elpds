#!/usr/bin/env python3
"""Create a disposable FAT16 hard disk for DOSBox-X, without a DOS installation."""

from pathlib import Path
import struct
import subprocess

image = Path(__file__).resolve().parent / "demo.img"
heads, sectors, cylinders = 16, 63, 65
total = heads * sectors * cylinders
start = sectors
mbr = bytearray(512)
entry = struct.pack(
    "<B3sB3sII",
    0x80, bytes((1, 1, 0)), 6, bytes((15, 63, 64)),
    start, total - start,
)
mbr[446:462] = entry
mbr[510:512] = b"\x55\xaa"
with image.open("wb") as disk:
    disk.truncate(total * 512)
    disk.write(mbr)
subprocess.run(
    ["mformat", "-i", f"{image}@@{start * 512}", "-T", str(total - start),
     "-h", str(heads), "-s", str(sectors), "-c", "4", "-H", str(start),
     "-v", "ELPDSDEMO", "::"],
    check=True,
)
print(f"Created {image} (FAT16, {total * 512 // 1024} KiB).")
