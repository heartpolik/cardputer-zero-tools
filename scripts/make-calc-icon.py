#!/usr/bin/env python3
"""Generates the calc icon PNG (default 100x100 RGBA) without PIL.

usage: make-calc-icon.py OUT.png [SIZE]"""
import math, struct, sys, zlib

W = H = int(sys.argv[2]) if len(sys.argv) > 2 else 100
U = W / 100.0  # drawing is designed on a 100x100 grid
SS = 4  # supersampling for antialiasing
BG = (24, 26, 31); BORDER = (60, 64, 72); GRID = (44, 48, 56); AXIS = (120, 126, 136)
C1 = (99, 226, 183); C2 = (112, 192, 232)

def inside_round_rect(x, y, r=18):
    cx = min(max(x, r), 100 - r); cy = min(max(y, r), 100 - r)
    return (x - cx) ** 2 + (y - cy) ** 2 <= r * r

def color_at(x, y):
    if not inside_round_rect(x, y):
        return None
    if not inside_round_rect(x, y, 18) or not (2 <= x <= 98 and 2 <= y <= 98):
        return BORDER
    if not inside_round_rect(x - 0, y - 0, 18):
        return BORDER
    # curves: y = sin, y = x^2/ parabola
    u = (x - 50) / 38 * math.pi
    s1 = 50 - 26 * math.sin(u)
    s2 = 50 + 30 - ((x - 50) / 22) ** 2 * 12
    if abs(y - s1) < 3.2:
        return C1
    if abs(y - s2) < 2.4 and 14 < x < 86:
        return C2
    if abs(x - 50) < 1.0 or abs(y - 50) < 1.0:
        return AXIS
    if (int(x) % 12 == 2 or int(y) % 12 == 2) and 8 < x < 92 and 8 < y < 92:
        return GRID
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
        n = SS * SS
        a = acc[3] // n
        if a:
            row += bytes([acc[0] * 255 // acc[3], acc[1] * 255 // acc[3], acc[2] * 255 // acc[3], a])
        else:
            row += bytes(4)
    rows.append(bytes(row))

def chunk(t, d):
    return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xffffffff)

png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", W, H, 8, 6, 0, 0, 0)) \
    + chunk(b"IDAT", zlib.compress(b"".join(rows), 9)) + chunk(b"IEND", b"")
open(sys.argv[1], "wb").write(png)
