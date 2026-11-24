#!/usr/bin/env python3
"""Generates the BitPuter icon PNG (default 100x100 RGBA) without PIL.

A 4x4 grid of bits (0xB5, 0x3C as nibbles) on the CalcPuter background.
usage: make-bit-icon.py OUT.png [SIZE]"""
import struct, sys, zlib

W = H = int(sys.argv[2]) if len(sys.argv) > 2 else 100
U = W / 100.0
SS = 4
BG = (24, 26, 31); BORDER = (60, 64, 72); OFF = (44, 48, 56)
ON = (99, 226, 183); ON2 = (112, 192, 232)
BITS = ["1011", "0101", "0011", "1100"]

def inside_round_rect(x, y, x0, y0, x1, y1, r):
    cx = min(max(x, x0 + r), x1 - r); cy = min(max(y, y0 + r), y1 - r)
    return (x - cx) ** 2 + (y - cy) ** 2 <= r * r

def color_at(x, y):
    if not inside_round_rect(x, y, 0, 0, 100, 100, 18):
        return None
    if not inside_round_rect(x, y, 2, 2, 98, 98, 16):
        return BORDER
    # cells: 16 px with 4 px gaps, grid 76 px wide centred
    for r, row in enumerate(BITS):
        for c, b in enumerate(row):
            x0 = 12 + c * 20; y0 = 12 + r * 20
            if inside_round_rect(x, y, x0, y0, x0 + 16, y0 + 16, 3):
                if b == "1":
                    return ON if r < 2 else ON2
                return OFF
    return BG

rows = []
for py in range(H):
    row = bytearray([0])
    for px in range(W):
        acc = [0, 0, 0, 0]
        for sy in range(SS):
            for sx in range(SS):
                c = color_at((px + (sx + .5) / SS) / U, (py + (sy + .5) / SS) / U)
                if c:
                    acc[0] += c[0]; acc[1] += c[1]; acc[2] += c[2]; acc[3] += 255
        a = acc[3] // (SS * SS)
        row += bytes([acc[0] * 255 // acc[3], acc[1] * 255 // acc[3], acc[2] * 255 // acc[3], a]) if a else bytes(4)
    rows.append(bytes(row))

def chunk(t, d):
    return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xffffffff)

png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", W, H, 8, 6, 0, 0, 0)) \
    + chunk(b"IDAT", zlib.compress(b"".join(rows), 9)) + chunk(b"IEND", b"")
open(sys.argv[1], "wb").write(png)
