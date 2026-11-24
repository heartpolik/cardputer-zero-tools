#!/usr/bin/env python3
"""Generates the OhmPuter icon PNG (default 100x100 RGBA) without PIL.

A resistor (4.7 kOhm: yellow violet red gold) on the CalcPuter background.
usage: make-ohm-icon.py OUT.png [SIZE]"""
import struct, sys, zlib

W = H = int(sys.argv[2]) if len(sys.argv) > 2 else 100
U = W / 100.0
SS = 4
BG = (24, 26, 31); BORDER = (60, 64, 72); OFF = (44, 48, 56)
LEAD = (150, 156, 166); BODY = (216, 195, 154)
BANDS = [(30, (253, 216, 53)), (42, (142, 36, 170)), (54, (229, 57, 53)), (70, (212, 175, 55))]

def inside_round_rect(x, y, x0, y0, x1, y1, r):
    cx = min(max(x, x0 + r), x1 - r); cy = min(max(y, y0 + r), y1 - r)
    return (x - cx) ** 2 + (y - cy) ** 2 <= r * r

def color_at(x, y):
    if not inside_round_rect(x, y, 0, 0, 100, 100, 18):
        return None
    if not inside_round_rect(x, y, 2, 2, 98, 98, 16):
        return BORDER
    if inside_round_rect(x, y, 20, 36, 80, 64, 9):
        for bx, col in BANDS:
            if bx <= x < bx + 7:
                return col
        return BODY
    if abs(y - 50) < 2.2 and 8 < x < 92:
        return LEAD
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
