#!/usr/bin/env python3
"""Generates the SerialPuter icon PNG (default 100x100 RGBA) without PIL.

A DB9 serial connector under a square-wave bit train on the CalcPuter background.
usage: make-serial-icon.py OUT.png [SIZE]"""
import struct, sys, zlib

W = H = int(sys.argv[2]) if len(sys.argv) > 2 else 100
U = W / 100.0
SS = 4
BG = (24, 26, 31); BORDER = (60, 64, 72)
SHELL = (44, 48, 56); EDGE = (99, 226, 183); PIN = (230, 196, 90); WAVE = (99, 226, 183)

def inside_round_rect(x, y, x0, y0, x1, y1, r):
    cx = min(max(x, x0 + r), x1 - r); cy = min(max(y, y0 + r), y1 - r)
    return (x - cx) ** 2 + (y - cy) ** 2 <= r * r

def in_trapezoid(x, y, top, bottom, y0, y1, inset=0):
    if y < y0 + inset or y > y1 - inset:
        return False
    t = (y - y0) / (y1 - y0)
    half = (top + (bottom - top) * t) / 2 - inset
    return abs(x - 50) <= half

# start bit, then 1 0 1 1 0 0 1 0, stop: high = 18, low = 28
BITS = [1, 0, 1, 0, 1, 1, 0, 0, 1, 0, 1, 1]
def wave(x, y):
    if not 14 <= x <= 86:
        return False
    i = min(int((x - 14) / 6), len(BITS) - 1)
    level = 18 if BITS[i] else 28
    if abs(y - level) <= 1.6:
        return True
    # vertical edges
    edge_x = 14 + 6 * round((x - 14) / 6)
    j = int(round((x - 14) / 6))
    if 0 < j < len(BITS) and BITS[j] != BITS[j - 1] and abs(x - edge_x) <= 1.6 and 18 <= y <= 28:
        return True
    return False

def color_at(x, y):
    if not inside_round_rect(x, y, 0, 0, 100, 100, 18):
        return None
    if not inside_round_rect(x, y, 2, 2, 98, 98, 16):
        return BORDER
    if wave(x, y):
        return WAVE
    if in_trapezoid(x, y, 72, 56, 40, 80):
        if not in_trapezoid(x, y, 72, 56, 40, 80, inset=3):
            return EDGE
        for px in (26, 38, 50, 62, 74):
            if (x - px) ** 2 + (y - 52) ** 2 <= 16:
                return PIN
        for px in (32, 44, 56, 68):
            if (x - px) ** 2 + (y - 66) ** 2 <= 16:
                return PIN
        return SHELL
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
