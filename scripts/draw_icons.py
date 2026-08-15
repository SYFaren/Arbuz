#!/usr/bin/env python3
"""Procedural Arbuz icons.

Every pixel is authored here as geometry. Re-run after edits:

    python3 scripts/draw_icons.py
"""

from __future__ import annotations

import math
import struct
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "resources" / "icons"

RIND = (18, 64, 28)
RIND_STRIPE = (8, 28, 14)
RIND_EDGE = (52, 108, 58)
PITH = (245, 236, 220)
FLESH = (176, 34, 46)
FLESH_DEEP = (132, 22, 34)
WHITE = (255, 255, 255)

UI_IDS = (
    "new",
    "open",
    "save",
    "save-as",
    "quit",
    "cut",
    "copy",
    "paste",
    "clear",
    "find",
    "goto",
    "fx",
    "bold",
    "italic",
    "text-color",
    "fill-color",
    "add-sheet",
    "delete-sheet",
    "rename-sheet",
    "settings",
    "theme",
    "credits",
    "about",
    "wizard",
)


def clamp(v: float, lo: float = 0.0, hi: float = 1.0) -> float:
    return lo if v < lo else hi if v > hi else v


def cover(dist: float, width: float = 1.0) -> float:
    if width <= 1e-6:
        return 1.0 if dist < 0 else 0.0
    return clamp(0.5 - dist / width)


def sd_circle(px: float, py: float, cx: float, cy: float, r: float) -> float:
    return math.hypot(px - cx, py - cy) - r


def sd_ellipse(px: float, py: float, cx: float, cy: float, ang: float, rx: float, ry: float) -> float:
    ca, sa = math.cos(ang), math.sin(ang)
    dx, dy = px - cx, py - cy
    lx = dx * ca + dy * sa
    ly = -dx * sa + dy * ca
    k = math.hypot(lx / max(rx, 1e-6), ly / max(ry, 1e-6))
    return (k - 1.0) * min(rx, ry)


def sd_round_box(px: float, py: float, cx: float, cy: float, hx: float, hy: float, rad: float) -> float:
    ax = abs(px - cx) - hx + rad
    ay = abs(py - cy) - hy + rad
    return math.hypot(max(ax, 0.0), max(ay, 0.0)) + min(max(ax, ay), 0.0) - rad


def sd_capsule(px: float, py: float, ax: float, ay: float, bx: float, by: float, r: float) -> float:
    pax, pay = px - ax, py - ay
    bax, bay = bx - ax, by - ay
    den = bax * bax + bay * bay
    h = 0.0 if den <= 1e-12 else clamp((pax * bax + pay * bay) / den)
    return math.hypot(pax - bax * h, pay - bay * h) - r


def sd_ring(px: float, py: float, cx: float, cy: float, r: float, t: float) -> float:
    return abs(math.hypot(px - cx, py - cy) - r) - t


class Canvas:
    def __init__(self, size: int) -> None:
        self.n = size
        self.px = [0.0] * (size * size * 4)

    def blend(self, x: int, y: int, rgb: tuple[int, int, int], a: float) -> None:
        if a <= 1e-4 or x < 0 or y < 0 or x >= self.n or y >= self.n:
            return
        i = (y * self.n + x) * 4
        da = self.px[i + 3]
        out_a = a + da * (1.0 - a)
        if out_a <= 1e-8:
            return
        sr, sg, sb = rgb[0] / 255.0, rgb[1] / 255.0, rgb[2] / 255.0
        ia = da * (1.0 - a)
        self.px[i] = (sr * a + self.px[i] * ia) / out_a
        self.px[i + 1] = (sg * a + self.px[i + 1] * ia) / out_a
        self.px[i + 2] = (sb * a + self.px[i + 2] * ia) / out_a
        self.px[i + 3] = out_a

    def rgba_bytes(self) -> bytes:
        out = bytearray(self.n * self.n * 4)
        for i in range(self.n * self.n):
            j = i * 4
            out[j] = int(clamp(self.px[j]) * 255.0 + 0.5)
            out[j + 1] = int(clamp(self.px[j + 1]) * 255.0 + 0.5)
            out[j + 2] = int(clamp(self.px[j + 2]) * 255.0 + 0.5)
            out[j + 3] = int(clamp(self.px[j + 3]) * 255.0 + 0.5)
        return bytes(out)


def box_down(src: Canvas, dst_size: int) -> Canvas:
    factor = src.n // dst_size
    assert src.n == dst_size * factor
    dst = Canvas(dst_size)
    inv = 1.0 / (factor * factor)
    for y in range(dst_size):
        for x in range(dst_size):
            acc = [0.0, 0.0, 0.0, 0.0]
            for oy in range(factor):
                for ox in range(factor):
                    i = ((y * factor + oy) * src.n + (x * factor + ox)) * 4
                    a = src.px[i + 3]
                    acc[0] += src.px[i] * a
                    acc[1] += src.px[i + 1] * a
                    acc[2] += src.px[i + 2] * a
                    acc[3] += a
            j = (y * dst_size + x) * 4
            a = acc[3] * inv
            dst.px[j + 3] = a
            if a > 1e-8:
                dst.px[j] = (acc[0] * inv) / a
                dst.px[j + 1] = (acc[1] * inv) / a
                dst.px[j + 2] = (acc[2] * inv) / a
    return dst


def paint_app(n: int, logical: int) -> Canvas:
    """Watermelon cross-section. No grid — just the fruit."""
    c = Canvas(n)
    s = float(n)
    cx = cy = s * 0.5
    aa = 1.15 if logical <= 24 else 0.9
    r_rind = s * 0.46
    r_pith = s * 0.398
    use_pith = logical >= 20
    r_flesh = s * 0.372 if use_pith else s * 0.388
    use_stripes = logical >= 24
    use_specular = logical >= 32

    for y in range(n):
        py = y + 0.5
        for x in range(n):
            px = x + 0.5
            d_rind = sd_circle(px, py, cx, cy, r_rind)
            d_pith = sd_circle(px, py, cx, cy, r_pith)
            d_flesh = sd_circle(px, py, cx, cy, r_flesh)
            in_rind = max(d_rind, -d_pith) if use_pith else max(d_rind, -d_flesh)

            a_rind = cover(in_rind, aa)
            if a_rind > 0:
                rgb = RIND
                if use_stripes:
                    ang = math.atan2(py - cy, px - cx)
                    wave = 0.5 + 0.5 * math.sin(ang * 6.0 + 0.4)
                    wave = wave * wave * (3.0 - 2.0 * wave)
                    rgb = (
                        int(RIND_STRIPE[0] + (RIND[0] - RIND_STRIPE[0]) * wave),
                        int(RIND_STRIPE[1] + (RIND[1] - RIND_STRIPE[1]) * wave),
                        int(RIND_STRIPE[2] + (RIND[2] - RIND_STRIPE[2]) * wave),
                    )
                c.blend(x, y, rgb, a_rind)
                a_edge = cover(abs(d_rind) - 0.45, aa) * a_rind
                if a_edge > 0:
                    c.blend(x, y, RIND_EDGE, a_edge * 0.55)

            if use_pith:
                a_pith = cover(max(d_pith, -d_flesh), aa)
                if a_pith > 0:
                    c.blend(x, y, PITH, a_pith)

            a_flesh = cover(d_flesh, aa)
            if a_flesh <= 0:
                continue

            rr = math.hypot(px - cx, py - cy) / max(r_flesh, 1.0)
            deep = clamp((rr - 0.18) / 0.82)
            deep *= deep
            rgb_f = (
                int(FLESH[0] + (FLESH_DEEP[0] - FLESH[0]) * deep * 0.72),
                int(FLESH[1] + (FLESH_DEEP[1] - FLESH[1]) * deep * 0.72),
                int(FLESH[2] + (FLESH_DEEP[2] - FLESH[2]) * deep * 0.72),
            )
            c.blend(x, y, rgb_f, a_flesh)

            if use_specular:
                a_sp = cover(
                    sd_ellipse(
                        px, py, cx - r_flesh * 0.28, cy - r_flesh * 0.34,
                        math.radians(-28), r_flesh * 0.34, r_flesh * 0.16,
                    ),
                    aa,
                ) * a_flesh
                if a_sp > 0:
                    c.blend(x, y, WHITE, a_sp * 0.16)

    return c


def paint_chip_bg(c: Canvas, n: int, logical: int) -> None:
    s = float(n)
    cx = cy = s * 0.5
    rad = s * 0.18
    hx = hy = s * 0.42
    aa = 1.1 if logical <= 24 else 0.9
    for y in range(n):
        py = y + 0.5
        for x in range(n):
            px = x + 0.5
            d = sd_round_box(px, py, cx, cy, hx, hy, rad)
            a = cover(d, aa)
            if a > 0:
                c.blend(x, y, PITH, a)
            a_b = cover(abs(d) - max(0.9, s * 0.045), aa)
            if a_b > 0:
                c.blend(x, y, RIND, a_b * 0.95)


def glyph_sdf(kind: str, px: float, py: float, s: float) -> tuple[float, float]:
    """Return (ink_sdf, accent_sdf). Negative is inside."""
    inf = 1e9
    d, a = inf, inf
    w = max(s * 0.055, 1.15)

    if kind == "new":
        d = min(d, abs(sd_round_box(px, py, s * 0.5, s * 0.52, s * 0.18, s * 0.22, s * 0.04)) - w * 0.55)
        d = min(d, sd_capsule(px, py, s * 0.5, s * 0.42, s * 0.5, s * 0.62, w))
        d = min(d, sd_capsule(px, py, s * 0.40, s * 0.52, s * 0.60, s * 0.52, w))
    elif kind == "open":
        d = min(d, abs(sd_round_box(px, py, s * 0.5, s * 0.56, s * 0.22, s * 0.16, s * 0.05)) - w * 0.5)
        d = min(d, sd_round_box(px, py, s * 0.38, s * 0.36, s * 0.10, s * 0.06, s * 0.04))
        a = sd_round_box(px, py, s * 0.5, s * 0.60, s * 0.20, s * 0.10, s * 0.04)
    elif kind == "save":
        d = min(d, sd_capsule(px, py, s * 0.32, s * 0.70, s * 0.68, s * 0.70, w * 1.1))
        d = min(d, sd_capsule(px, py, s * 0.32, s * 0.70, s * 0.32, s * 0.58, w * 1.1))
        d = min(d, sd_capsule(px, py, s * 0.68, s * 0.70, s * 0.68, s * 0.58, w * 1.1))
        a = min(a, sd_capsule(px, py, s * 0.5, s * 0.28, s * 0.5, s * 0.58, w * 1.05))
        a = min(a, sd_capsule(px, py, s * 0.5, s * 0.58, s * 0.38, s * 0.46, w * 0.95))
        a = min(a, sd_capsule(px, py, s * 0.5, s * 0.58, s * 0.62, s * 0.46, w * 0.95))
    elif kind == "save-as":
        d = min(d, sd_capsule(px, py, s * 0.28, s * 0.70, s * 0.58, s * 0.70, w * 1.05))
        d = min(d, sd_capsule(px, py, s * 0.28, s * 0.70, s * 0.28, s * 0.58, w * 1.05))
        a = min(a, sd_capsule(px, py, s * 0.42, s * 0.30, s * 0.42, s * 0.56, w))
        a = min(a, sd_capsule(px, py, s * 0.42, s * 0.56, s * 0.32, s * 0.46, w * 0.9))
        a = min(a, sd_capsule(px, py, s * 0.42, s * 0.56, s * 0.52, s * 0.46, w * 0.9))
        d = min(d, sd_capsule(px, py, s * 0.52, s * 0.38, s * 0.72, s * 0.38, w * 0.95))
        d = min(d, sd_capsule(px, py, s * 0.72, s * 0.38, s * 0.62, s * 0.28, w * 0.9))
        d = min(d, sd_capsule(px, py, s * 0.72, s * 0.38, s * 0.62, s * 0.48, w * 0.9))
    elif kind == "quit":
        d = min(d, abs(sd_round_box(px, py, s * 0.42, s * 0.5, s * 0.14, s * 0.20, s * 0.04)) - w * 0.5)
        a = min(a, sd_capsule(px, py, s * 0.40, s * 0.5, s * 0.74, s * 0.5, w * 1.05))
        a = min(a, sd_capsule(px, py, s * 0.74, s * 0.5, s * 0.62, s * 0.38, w * 0.95))
        a = min(a, sd_capsule(px, py, s * 0.74, s * 0.5, s * 0.62, s * 0.62, w * 0.95))
    elif kind == "cut":
        d = min(d, sd_capsule(px, py, s * 0.34, s * 0.40, s * 0.64, s * 0.72, w * 0.88))
        d = min(d, sd_capsule(px, py, s * 0.66, s * 0.40, s * 0.36, s * 0.72, w * 0.88))
        d = min(d, sd_ring(px, py, s * 0.30, s * 0.30, s * 0.08, w * 0.62))
        d = min(d, sd_ring(px, py, s * 0.70, s * 0.30, s * 0.08, w * 0.62))
    elif kind == "copy":
        d = min(d, abs(sd_round_box(px, py, s * 0.44, s * 0.46, s * 0.14, s * 0.18, s * 0.04)) - w * 0.5)
        d = min(d, abs(sd_round_box(px, py, s * 0.56, s * 0.56, s * 0.14, s * 0.18, s * 0.04)) - w * 0.5)
    elif kind == "paste":
        d = min(d, abs(sd_round_box(px, py, s * 0.5, s * 0.56, s * 0.18, s * 0.18, s * 0.04)) - w * 0.5)
        d = min(d, sd_round_box(px, py, s * 0.5, s * 0.32, s * 0.10, s * 0.07, s * 0.035))
        a = sd_round_box(px, py, s * 0.5, s * 0.58, s * 0.10, s * 0.08, s * 0.03)
    elif kind == "clear":
        d = min(d, abs(sd_round_box(px, py, s * 0.5, s * 0.5, s * 0.18, s * 0.18, s * 0.04)) - w * 0.5)
        a = sd_capsule(px, py, s * 0.34, s * 0.34, s * 0.66, s * 0.66, w * 1.05)
    elif kind == "find":
        d = min(d, sd_ring(px, py, s * 0.44, s * 0.44, s * 0.14, w * 0.7))
        d = min(d, sd_capsule(px, py, s * 0.54, s * 0.54, s * 0.72, s * 0.72, w * 1.05))
    elif kind == "goto":
        d = min(d, abs(sd_round_box(px, py, s * 0.58, s * 0.58, s * 0.12, s * 0.12, s * 0.03)) - w * 0.45)
        a = min(a, sd_capsule(px, py, s * 0.28, s * 0.28, s * 0.50, s * 0.50, w * 1.0))
        a = min(a, sd_capsule(px, py, s * 0.50, s * 0.50, s * 0.38, s * 0.50, w * 0.9))
        a = min(a, sd_capsule(px, py, s * 0.50, s * 0.50, s * 0.50, s * 0.38, w * 0.9))
    elif kind == "fx":
        stem_x = s * 0.32 + (0.5 * s - py) * 0.06
        d = min(d, sd_capsule(px, py, stem_x, s * 0.28, stem_x - s * 0.02, s * 0.74, w))
        d = min(d, sd_capsule(px, py, stem_x, s * 0.30, stem_x + s * 0.16, s * 0.24, w * 0.92))
        d = min(d, sd_capsule(px, py, stem_x - s * 0.10, s * 0.46, stem_x + s * 0.12, s * 0.44, w * 0.82))
        xx, xy, arm = s * 0.68, s * 0.62, s * 0.10
        d = min(d, sd_capsule(px, py, xx - arm, xy - arm, xx + arm, xy + arm, w * 0.78))
        d = min(d, sd_capsule(px, py, xx - arm, xy + arm, xx + arm, xy - arm, w * 0.78))
    elif kind == "bold":
        d = min(d, sd_capsule(px, py, s * 0.36, s * 0.26, s * 0.36, s * 0.74, w * 1.25))
        d = min(d, sd_capsule(px, py, s * 0.36, s * 0.26, s * 0.62, s * 0.26, w * 1.05))
        d = min(d, sd_capsule(px, py, s * 0.36, s * 0.50, s * 0.58, s * 0.50, w * 1.0))
        d = min(d, sd_capsule(px, py, s * 0.36, s * 0.74, s * 0.62, s * 0.74, w * 1.05))
        d = min(d, sd_capsule(px, py, s * 0.62, s * 0.26, s * 0.62, s * 0.50, w * 0.95))
        d = min(d, sd_capsule(px, py, s * 0.62, s * 0.50, s * 0.62, s * 0.74, w * 0.95))
    elif kind == "italic":
        d = min(d, sd_capsule(px, py, s * 0.58, s * 0.26, s * 0.40, s * 0.74, w * 1.2))
        d = min(d, sd_capsule(px, py, s * 0.46, s * 0.26, s * 0.68, s * 0.26, w * 0.9))
        d = min(d, sd_capsule(px, py, s * 0.30, s * 0.74, s * 0.52, s * 0.74, w * 0.9))
    elif kind == "text-color":
        d = min(d, sd_capsule(px, py, s * 0.50, s * 0.26, s * 0.34, s * 0.62, w * 0.95))
        d = min(d, sd_capsule(px, py, s * 0.50, s * 0.26, s * 0.66, s * 0.62, w * 0.95))
        d = min(d, sd_capsule(px, py, s * 0.40, s * 0.50, s * 0.60, s * 0.50, w * 0.8))
        a = sd_capsule(px, py, s * 0.30, s * 0.74, s * 0.70, s * 0.74, w * 1.15)
    elif kind == "fill-color":
        d = min(d, abs(sd_round_box(px, py, s * 0.5, s * 0.48, s * 0.20, s * 0.16, s * 0.04)) - w * 0.45)
        a = sd_round_box(px, py, s * 0.5, s * 0.48, s * 0.16, s * 0.12, s * 0.03)
        d = min(d, sd_capsule(px, py, s * 0.42, s * 0.66, s * 0.50, s * 0.78, w * 0.85))
    elif kind == "add-sheet":
        d = min(d, sd_capsule(px, py, s * 0.30, s * 0.5, s * 0.70, s * 0.5, w * 1.15))
        d = min(d, sd_capsule(px, py, s * 0.5, s * 0.30, s * 0.5, s * 0.70, w * 1.15))
    elif kind == "delete-sheet":
        a = sd_capsule(px, py, s * 0.30, s * 0.5, s * 0.70, s * 0.5, w * 1.2)
    elif kind == "rename-sheet":
        d = min(d, abs(sd_round_box(px, py, s * 0.42, s * 0.56, s * 0.16, s * 0.14, s * 0.03)) - w * 0.45)
        a = sd_capsule(px, py, s * 0.48, s * 0.66, s * 0.72, s * 0.30, w * 0.95)
        a = min(a, sd_circle(px, py, s * 0.72, s * 0.30, w * 1.1))
    elif kind == "settings":
        for i, (yy, x0, x1, knob) in enumerate(
            ((0.32, 0.28, 0.70, 0.58), (0.50, 0.28, 0.70, 0.40), (0.68, 0.28, 0.70, 0.64))
        ):
            d = min(d, sd_capsule(px, py, s * x0, s * yy, s * x1, s * yy, w * 0.72))
            a = min(a, sd_circle(px, py, s * knob, s * yy, w * 1.35))
    elif kind == "theme":
        d = min(d, sd_ring(px, py, s * 0.5, s * 0.5, s * 0.22, w * 0.85))
        a = sd_circle(px, py, s * 0.5, s * 0.5, s * 0.15)
    elif kind == "credits":
        d = min(d, sd_capsule(px, py, s * 0.30, s * 0.34, s * 0.70, s * 0.34, w * 0.95))
        d = min(d, sd_capsule(px, py, s * 0.30, s * 0.50, s * 0.70, s * 0.50, w * 0.95))
        d = min(d, sd_capsule(px, py, s * 0.30, s * 0.66, s * 0.58, s * 0.66, w * 0.95))
    elif kind == "about":
        d = min(d, sd_ring(px, py, s * 0.5, s * 0.5, s * 0.22, w * 0.75))
        d = min(d, sd_circle(px, py, s * 0.5, s * 0.36, w * 1.15))
        d = min(d, sd_capsule(px, py, s * 0.5, s * 0.48, s * 0.5, s * 0.68, w * 1.05))
    elif kind == "wizard":
        a = min(a, sd_circle(px, py, s * 0.50, s * 0.34, s * 0.07))
        a = min(a, sd_circle(px, py, s * 0.34, s * 0.62, s * 0.06))
        a = min(a, sd_circle(px, py, s * 0.66, s * 0.62, s * 0.06))
        d = min(d, sd_capsule(px, py, s * 0.50, s * 0.40, s * 0.38, s * 0.58, w * 0.55))
        d = min(d, sd_capsule(px, py, s * 0.50, s * 0.40, s * 0.62, s * 0.58, w * 0.55))
    return d, a


def paint_ui(kind: str, n: int, logical: int) -> Canvas:
    c = Canvas(n)
    paint_chip_bg(c, n, logical)
    s = float(n)
    aa = 1.05 if logical > 20 else 1.2
    for y in range(n):
        py = y + 0.5
        for x in range(n):
            px = x + 0.5
            d_ink, d_acc = glyph_sdf(kind, px, py, s)
            a_ink = cover(d_ink, aa)
            if a_ink > 0:
                c.blend(x, y, RIND, a_ink)
            a_acc = cover(d_acc, aa)
            if a_acc > 0:
                c.blend(x, y, FLESH, a_acc)
    return c


def render(paint, logical: int) -> Canvas:
    scale = 4 if logical <= 32 else (2 if logical <= 64 else 1)
    src = paint(logical * scale, logical)
    if scale > 1:
        return box_down(src, logical)
    return src


def write_png(path: Path, w: int, h: int, rgba: bytes) -> None:
    def chunk(tag: bytes, data: bytes) -> bytes:
        crc = zlib.crc32(tag + data) & 0xFFFFFFFF
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", crc)

    raw = bytearray()
    stride = w * 4
    for y in range(h):
        raw.append(0)
        raw.extend(rgba[y * stride : (y + 1) * stride])
    ihdr = struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) + chunk(b"IDAT", zlib.compress(bytes(raw), 9)) + chunk(b"IEND", b"")
    path.write_bytes(png)


def write_ico(path: Path, images: list[tuple[int, bytes]]) -> None:
    """Classic BMP-in-ICO so MinGW windres can embed it when cross-compiling."""
    blobs: list[bytes] = []
    for w, rgba in images:
        h = w
        xor = bytearray()
        for y in range(h - 1, -1, -1):
            for x in range(w):
                i = (y * w + x) * 4
                r, g, b, a = rgba[i], rgba[i + 1], rgba[i + 2], rgba[i + 3]
                xor.extend((b, g, r, a))
        row_and = ((w + 31) // 32) * 4
        mask = bytes(row_and * h)
        hdr = struct.pack("<IIIHHIIIIII", 40, w, h * 2, 1, 32, 0, len(xor), 0, 0, 0, 0)
        blobs.append(hdr + bytes(xor) + mask)

    count = len(blobs)
    header = struct.pack("<HHH", 0, 1, count)
    offset = 6 + 16 * count
    entries = b""
    payload = b""
    for (w, _), data in zip(images, blobs):
        wb = 0 if w >= 256 else w
        entries += struct.pack("<BBBBHHII", wb, wb, 0, 0, 1, 32, len(data), offset)
        offset += len(data)
        payload += data
    path.write_bytes(header + entries + payload)


def write_svg(path: Path) -> None:
    n = 512.0
    cx = cy = n * 0.5
    r_rind, r_pith, r_flesh = n * 0.46, n * 0.398, n * 0.372
    hex_c = lambda rgb: "#{:02X}{:02X}{:02X}".format(*rgb)
    lines = [
        f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {int(n)} {int(n)}" width="{int(n)}" height="{int(n)}">',
        "  <!-- Generated by scripts/draw_icons.py. Geometry is original. -->",
        "  <defs>",
        f'    <clipPath id="flesh"><circle cx="{cx}" cy="{cy}" r="{r_flesh}"/></clipPath>',
        '    <clipPath id="rindRing">',
        f'      <path fill-rule="evenodd" d="M {cx} {cy - r_rind} A {r_rind} {r_rind} 0 1 1 {cx} {cy + r_rind} A {r_rind} {r_rind} 0 1 1 {cx} {cy - r_rind} Z',
        f'            M {cx} {cy - r_pith} A {r_pith} {r_pith} 0 1 0 {cx} {cy + r_pith} A {r_pith} {r_pith} 0 1 0 {cx} {cy - r_pith} Z"/>',
        "    </clipPath>",
        "  </defs>",
        f'  <circle cx="{cx}" cy="{cy}" r="{r_rind}" fill="{hex_c(RIND)}"/>',
        '  <g clip-path="url(#rindRing)">',
    ]
    for i in range(10):
        a0 = -math.pi + i * (2 * math.pi / 10)
        a1 = a0 + (2 * math.pi / 10) * 0.42
        x0, y0 = cx + math.cos(a0) * r_rind, cy + math.sin(a0) * r_rind
        x1, y1 = cx + math.cos(a1) * r_rind, cy + math.sin(a1) * r_rind
        lines.append(
            f'    <path fill="{hex_c(RIND_STRIPE)}" d="M {cx} {cy} L {x0:.1f} {y0:.1f} A {r_rind} {r_rind} 0 0 1 {x1:.1f} {y1:.1f} Z"/>'
        )
    lines += [
        "  </g>",
        f'  <circle cx="{cx}" cy="{cy}" r="{r_rind}" fill="none" stroke="{hex_c(RIND_EDGE)}" stroke-width="3"/>',
        f'  <circle cx="{cx}" cy="{cy}" r="{(r_pith + r_flesh) * 0.5}" fill="none" stroke="{hex_c(PITH)}" stroke-width="{r_pith - r_flesh}"/>',
        f'  <circle cx="{cx}" cy="{cy}" r="{r_flesh}" fill="{hex_c(FLESH)}"/>',
        '  <g clip-path="url(#flesh)">',
        f'    <ellipse cx="{cx - r_flesh * 0.28:.1f}" cy="{cy - r_flesh * 0.34:.1f}" '
        f'rx="{r_flesh * 0.34:.1f}" ry="{r_flesh * 0.16:.1f}" transform="rotate(-28 {cx} {cy})" '
        f'fill="#FFFFFF" fill-opacity="0.16"/>',
        "  </g>",
        "</svg>",
        "",
    ]
    path.write_text("\n".join(lines), encoding="utf-8")


def save_png(path: Path, canvas: Canvas) -> bytes:
    data = canvas.rgba_bytes()
    write_png(path, canvas.n, canvas.n, data)
    return path.read_bytes()


def write_qrc_snippet() -> None:
    qrc = ROOT / "resources" / "arbuz.qrc"
    ui_files = []
    for stem in UI_IDS:
        for size in (16, 32, 64):
            ui_files.append(f"        <file>icons/{stem}-{size}.png</file>")
    app_files = [
        "        <file>icons/arbuz-16.png</file>",
        "        <file>icons/arbuz-24.png</file>",
        "        <file>icons/arbuz-32.png</file>",
        "        <file>icons/arbuz-48.png</file>",
        "        <file>icons/arbuz-64.png</file>",
        "        <file>icons/arbuz-128.png</file>",
        "        <file>icons/arbuz-256.png</file>",
    ]
    text = (
        "<RCC>\n"
        '    <qresource prefix="/arbuz">\n'
        "        <file>themes/arbuz.json</file>\n"
        "        <file>themes/white.json</file>\n"
        "        <file>themes/dark.json</file>\n"
        '        <file alias="CREDITS.md">../CREDITS.md</file>\n'
        + "\n".join(app_files)
        + "\n"
        + "\n".join(ui_files)
        + "\n"
        "    </qresource>\n"
        "</RCC>\n"
    )
    qrc.write_text(text, encoding="utf-8")


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    # Drop leftover grid-era names if present.
    for leftover in ("fx-32.png", "fx-64.png", "add-sheet-32.png", "add-sheet-64.png"):
        pass
    app_sizes = (16, 24, 32, 48, 64, 128, 256)
    ico_rgba: list[tuple[int, bytes]] = []
    for size in app_sizes:
        print(f"app {size}")
        canvas = render(paint_app, size)
        save_png(OUT / f"arbuz-{size}.png", canvas)
        if size in (16, 24, 32, 48, 64, 256):
            ico_rgba.append((size, canvas.rgba_bytes()))
    write_ico(OUT / "arbuz.ico", ico_rgba)
    write_svg(OUT / "arbuz.svg")

    for stem in UI_IDS:
        for size in (16, 32, 64):
            print(f"{stem} {size}")
            save_png(OUT / f"{stem}-{size}.png", render(lambda n, logical, k=stem: paint_ui(k, n, logical), size))

    write_qrc_snippet()
    print("wrote", OUT)


if __name__ == "__main__":
    main()
