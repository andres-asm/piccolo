#!/usr/bin/env python3
"""Generate 32x32 pixel-art gamepad glyphs for piccolo's big picture GUI.

Every glyph is pure white with a 1-bit alpha mask, so the GUI can tint it to
any theme colour (multiply the texture by the colour). Labels are cut out of
the shapes, so they show the background through them.

Output: assets/glyphs/<pad>/<button>.png, button names follow SDL3 gamepad
naming with positional face buttons (south/east/west/north). See the README
written next to the glyphs.

No dependencies beyond the Python standard library.

    python3 tools/make_glyphs.py [output_dir] [--sheet sheet.png]
"""

import math
import os
import struct
import sys
import zlib

SIZE = 32

# ---------------------------------------------------------------------------
# fonts

FONT5x7 = {
    "A": [".###.", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"],
    "B": ["####.", "#...#", "#...#", "####.", "#...#", "#...#", "####."],
    "C": [".####", "#....", "#....", "#....", "#....", "#....", ".####"],
    "L": ["#....", "#....", "#....", "#....", "#....", "#....", "#####"],
    "R": ["####.", "#...#", "#...#", "####.", "#.#..", "#..#.", "#...#"],
    "S": [".####", "#....", "#....", ".###.", "....#", "....#", "####."],
    "T": ["#####", "..#..", "..#..", "..#..", "..#..", "..#..", "..#.."],
    "X": ["#...#", "#...#", ".#.#.", "..#..", ".#.#.", "#...#", "#...#"],
    "Y": ["#...#", "#...#", ".#.#.", "..#..", "..#..", "..#..", "..#.."],
    "Z": ["#####", "....#", "...#.", "..#..", ".#...", "#....", "#####"],
    "1": ["..#..", ".##..", "..#..", "..#..", "..#..", "..#..", ".###."],
    "2": [".###.", "#...#", "....#", "..##.", ".#...", "#....", "#####"],
    "3": ["####.", "....#", "....#", ".###.", "....#", "....#", "####."],
}

FONT3x5 = {
    "A": [".#.", "#.#", "###", "#.#", "#.#"],
    "B": ["##.", "#.#", "##.", "#.#", "##."],
    "C": [".##", "#..", "#..", "#..", ".##"],
    "E": ["###", "#..", "##.", "#..", "###"],
    "H": ["#.#", "#.#", "###", "#.#", "#.#"],
    "I": ["###", ".#.", ".#.", ".#.", "###"],
    "K": ["#.#", "#.#", "##.", "#.#", "#.#"],
    "L": ["#..", "#..", "#..", "#..", "###"],
    "N": ["##.", "#.#", "#.#", "#.#", "#.#"],
    "O": [".#.", "#.#", "#.#", "#.#", ".#."],
    "P": ["##.", "#.#", "##.", "#..", "#.."],
    "R": ["##.", "#.#", "##.", "#.#", "#.#"],
    "S": [".##", "#..", ".#.", "..#", "##."],
    "T": ["###", ".#.", ".#.", ".#.", ".#."],
    "1": [".#.", "##.", ".#.", ".#.", "###"],
    "2": ["##.", "..#", ".#.", "#..", "###"],
    "3": ["##.", "..#", ".#.", "..#", "##."],
    "4": ["#.#", "#.#", "###", "..#", "..#"],
    "5": ["###", "#..", "##.", "..#", "##."],
    "6": [".##", "#..", "###", "#.#", "###"],
    "7": ["###", "..#", ".#.", ".#.", ".#."],
    "8": ["###", "#.#", "###", "#.#", "###"],
    "9": ["###", "#.#", "###", "..#", "##."],
    "0": ["###", "#.#", "#.#", "#.#", "###"],
    "D": ["##.", "#.#", "#.#", "#.#", "##."],
    "G": [".##", "#..", "#.#", "#.#", ".##"],
    "M": ["#.#", "###", "###", "#.#", "#.#"],
    "U": ["#.#", "#.#", "#.#", "#.#", "###"],
    "V": ["#.#", "#.#", "#.#", "#.#", ".#."],
    "W": ["#.#", "#.#", "###", "###", "#.#"],
    "X": ["#.#", "#.#", ".#.", "#.#", "#.#"],
    "Y": ["#.#", "#.#", ".#.", ".#.", ".#."],
    "Z": ["###", "..#", ".#.", "#..", "###"],
    "F": ["###", "#..", "##.", "#..", "#.."],
    "J": ["..#", "..#", "..#", "#.#", ".#."],
    "Q": [".#.", "#.#", "#.#", "##.", ".##"],
    "_": ["...", "...", "...", "...", "###"],
    "-": ["...", "...", "###", "...", "..."],
    " ": ["...", "...", "...", "...", "..."],
}


# ---------------------------------------------------------------------------
# canvas

class Canvas:
    def __init__(self, w=SIZE, h=SIZE):
        self.w = w
        self.h = h
        self.p = [[0] * w for _ in range(h)]

    def set(self, x, y, v=1):
        if 0 <= x < self.w and 0 <= y < self.h:
            self.p[y][x] = v

    def get(self, x, y):
        if 0 <= x < self.w and 0 <= y < self.h:
            return self.p[y][x]
        return 0

    def fill_if(self, test, v=1):
        for y in range(self.h):
            for x in range(self.w):
                if test(x + 0.5, y + 0.5):
                    self.p[y][x] = v

    # shapes take pixel-center coordinates; (16, 16) is the glyph centre
    def disc(self, cx, cy, r, v=1):
        self.fill_if(lambda x, y: (x - cx) ** 2 + (y - cy) ** 2 <= r * r, v)

    def ring(self, cx, cy, r_out, r_in, v=1):
        def t(x, y):
            d = (x - cx) ** 2 + (y - cy) ** 2
            return r_in * r_in < d <= r_out * r_out
        self.fill_if(t, v)

    def rect(self, x0, y0, x1, y1, v=1):
        """Inclusive pixel rectangle."""
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                self.set(x, y, v)

    def rrect(self, x0, y0, x1, y1, r, v=1):
        """Inclusive pixel rectangle with rounded corners of radius r."""
        ax0, ay0, ax1, ay1 = x0 + r, y0 + r, x1 + 1 - r, y1 + 1 - r

        def t(x, y):
            if x < x0 or x > x1 + 1 or y < y0 or y > y1 + 1:
                return False
            nx = min(max(x, ax0), ax1)
            ny = min(max(y, ay0), ay1)
            return (x - nx) ** 2 + (y - ny) ** 2 <= r * r
        self.fill_if(t, v)

    def poly(self, pts, v=1):
        def t(x, y):
            inside = False
            n = len(pts)
            j = n - 1
            for i in range(n):
                xi, yi = pts[i]
                xj, yj = pts[j]
                if (yi > y) != (yj > y):
                    if x < (xj - xi) * (y - yi) / (yj - yi) + xi:
                        inside = not inside
                j = i
            return inside
        self.fill_if(t, v)

    def text(self, s, font, scale, cx, cy, v=0, spacing=1):
        """Draw text centred on (cx, cy) (pixel-edge coordinates)."""
        gw = len(next(iter(font.values()))[0])
        gh = len(next(iter(font.values())))
        tw = (len(s) * (gw + spacing) - spacing) * scale
        th = gh * scale
        ox = int(round(cx - tw / 2.0))
        oy = int(round(cy - th / 2.0))
        for i, ch in enumerate(s):
            rows = font[ch]
            bx = ox + i * (gw + spacing) * scale
            for ry, row in enumerate(rows):
                for rx, c in enumerate(row):
                    if c == "#":
                        self.rect(bx + rx * scale, oy + ry * scale,
                                  bx + rx * scale + scale - 1,
                                  oy + ry * scale + scale - 1, v)

    def outline(self, width=2):
        """Keep only the outer `width` pixels of the current shape."""
        mask = [row[:] for row in self.p]
        for _ in range(width):
            nxt = [[0] * self.w for _ in range(self.h)]
            for y in range(self.h):
                for x in range(self.w):
                    if mask[y][x]:
                        keep = 1
                        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1),
                                       (1, 1), (-1, -1), (1, -1), (-1, 1)):
                            xx, yy = x + dx, y + dy
                            if not (0 <= xx < self.w and 0 <= yy < self.h) \
                                    or not mask[yy][xx]:
                                keep = 0
                                break
                        nxt[y][x] = keep
            mask = nxt
        for y in range(self.h):
            for x in range(self.w):
                if mask[y][x]:
                    self.p[y][x] = 0

    def merge(self, other):
        for y in range(self.h):
            for x in range(self.w):
                if other.p[y][x]:
                    self.p[y][x] = 1


# ---------------------------------------------------------------------------
# png

def png_bytes(w, h, rgba_rows):
    raw = b"".join(b"\x00" + bytes(row) for row in rgba_rows)

    def chunk(tag, data):
        c = struct.pack(">I", len(data)) + tag + data
        return c + struct.pack(">I", zlib.crc32(tag + data) & 0xffffffff)

    return (b"\x89PNG\r\n\x1a\n"
            + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(raw, 9))
            + chunk(b"IEND", b""))


def save_canvas(c, path):
    rows = []
    for y in range(c.h):
        row = []
        for x in range(c.w):
            row += [255, 255, 255, 255 if c.p[y][x] else 0]
        rows.append(row)
    with open(path, "wb") as f:
        f.write(png_bytes(c.w, c.h, rows))


# ---------------------------------------------------------------------------
# glyph building blocks (each returns a fresh Canvas)

C = 16.0  # centre


def g_letter_button(label, r=14.5):
    c = Canvas()
    c.disc(C, C, r)
    c.text(label, FONT5x7, 2, C, C)
    return c


def g_sony(shape):
    c = Canvas()
    c.disc(C, C, 14.5)
    if shape == "cross":
        def t(x, y):
            dx, dy = x - C, y - C
            return (abs(dx - dy) < 2.3 or abs(dx + dy) < 2.3) \
                and max(abs(dx), abs(dy)) <= 7.5
        c.fill_if(t, 0)
    elif shape == "circle":
        c.ring(C, C, 8.5, 5.0, 0)
    elif shape == "square":
        c.rect(8, 8, 23, 23, 0)
        c.rect(11, 11, 20, 20, 1)
    elif shape == "triangle":
        c.poly([(C, 6.0), (25.5, 22.5), (6.5, 22.5)], 0)
        c.poly([(C, 11.5), (21.0, 19.8), (11.0, 19.8)], 1)
    return c


def g_bean(label, vertical):
    """GameCube X / Y kidney-bean button."""
    c = Canvas()
    ox, r0, r1, span = -8.0, 17.0, 31.0, math.radians(19.5)
    mid = (r0 + r1) / 2.0
    half = (r1 - r0) / 2.0

    def t(x, y):
        if vertical:
            # arch over A: concave side faces down
            x, y = SIZE - y, x
        dx, dy = x - ox, y - C
        d = math.hypot(dx, dy)
        a = math.atan2(dy, dx)
        if r0 <= d <= r1 and abs(a) <= span:
            return True
        for s in (-1, 1):
            ex = ox + mid * math.cos(s * span)
            ey = C + mid * math.sin(s * span)
            if (x - ex) ** 2 + (y - ey) ** 2 <= half * half:
                return True
        return False
    c.fill_if(t)
    lx = ox + mid
    if vertical:
        c.text(label, FONT5x7, 2, C, SIZE - lx)
    else:
        c.text(label, FONT5x7, 2, lx, C)
    return c


def g_bumper(label):
    c = Canvas()
    c.rrect(1, 6, 30, 25, 5)
    c.rrect(1, 10, 30, 25, 2)
    c.rect(1, 6, 30, 7, 0)
    c.rrect(1, 6, 30, 25, 5)
    c.text(label, FONT5x7, 2 if len(label) <= 2 else 1, C, C)
    return c


def g_trigger(label):
    c = Canvas()
    c.rrect(2, 1, 29, 30, 10)
    c.rrect(2, 14, 29, 30, 3)
    c.text(label, FONT5x7, 2, C, 17.5)
    return c


def g_stick(label, press=False, thumb=9.5):
    c = Canvas()
    if press:
        # dashed base ring = "click"
        def t(x, y):
            dx, dy = x - C, y - C
            d = math.hypot(dx, dy)
            if not (12.0 < d <= 15.0):
                return False
            a = (math.degrees(math.atan2(dy, dx)) + 360.0 + 22.5) % 45.0
            return abs(a - 22.5) < 9.0
        c.fill_if(t)
    else:
        c.ring(C, C, 15.0, 12.5)
    c.disc(C, C, thumb)
    if label:
        c.text(label, FONT5x7, 2 if thumb >= 9 else 1, C, C)
    return c


def _arrow(c, cx, cy, d, size, v):
    """Triangle arrow pointing in direction d (up/down/left/right)."""
    s = size
    if d == "up":
        pts = [(cx, cy - s), (cx + s, cy + s * 0.6), (cx - s, cy + s * 0.6)]
    elif d == "down":
        pts = [(cx, cy + s), (cx + s, cy - s * 0.6), (cx - s, cy - s * 0.6)]
    elif d == "left":
        pts = [(cx - s, cy), (cx + s * 0.6, cy - s), (cx + s * 0.6, cy + s)]
    else:
        pts = [(cx + s, cy), (cx - s * 0.6, cy - s), (cx - s * 0.6, cy + s)]
    c.poly(pts, v)


ARM_CENTRES = {"up": (16, 7), "down": (16, 25), "left": (7, 16), "right": (25, 16)}


def g_dpad_cross(active=None):
    """Cross d-pad. active: None = all solid, else set of solid arms."""
    full = Canvas()
    full.rect(11, 1, 20, 30)
    full.rect(1, 11, 30, 20)
    if active is None:
        c = full
        for d, (ax, ay) in ARM_CENTRES.items():
            _arrow(c, ax, ay, d, 3.0, 0)
        return c
    c = Canvas()
    c.merge(full)
    c.outline(2)
    arms = {"up": (11, 1, 20, 12), "down": (11, 19, 20, 30),
            "left": (1, 11, 12, 20), "right": (19, 11, 30, 20)}
    for d in active:
        x0, y0, x1, y1 = arms[d]
        c.rect(x0, y0, x1, y1)
        ax, ay = ARM_CENTRES[d]
        _arrow(c, ax, ay, d, 3.0, 0)
    return c


def _sony_piece(d):
    p = Canvas()
    up = [(11.0, 1.0), (21.0, 1.0), (21.0, 10.0), (16.0, 14.5), (11.0, 10.0)]
    if d == "up":
        pts = up
    elif d == "down":
        pts = [(x, 32.0 - y) for x, y in up]
    elif d == "left":
        pts = [(y, x) for x, y in up]
    else:
        pts = [(32.0 - y, x) for x, y in up]
    p.poly(pts)
    return p


def g_dpad_sony(active=None):
    c = Canvas()
    for d in ("up", "down", "left", "right"):
        piece = _sony_piece(d)
        solid = active is None or d in active
        if not solid:
            piece.outline(2)
        else:
            ax, ay = ARM_CENTRES[d]
            ax += {"up": 0, "down": 0, "left": -1, "right": 1}[d]
            ay += {"up": -1, "down": 1, "left": 0, "right": 0}[d]
            _arrow(piece, ax, ay, d, 2.6, 0)
        c.merge(piece)
    return c


def g_pill(word):
    c = Canvas()
    c.rrect(0, 9, 31, 22, 6)
    c.text(word, FONT3x5, 1, C, C)
    return c


def g_plusminus(sign, disc):
    c = Canvas()
    if disc:
        c.disc(C, C, 12.5)
        v = 0
        t = 4
        l = 8
    else:
        v = 1
        t = 8
        l = 13
    c.rect(16 - l, 16 - t // 2, 15 + l, 15 + t // 2, v)
    if sign == "+":
        c.rect(16 - t // 2, 16 - l, 15 + t // 2, 15 + l, v)
    return c


def g_home():
    c = Canvas()
    c.disc(C, C, 14.5)
    c.poly([(C, 5.5), (26.0, 15.5), (6.0, 15.5)], 0)
    c.rect(9, 15, 22, 24, 0)
    c.rect(14, 19, 17, 24, 1)
    return c


def g_disc_arrow(d):
    c = Canvas()
    c.disc(C, C, 12.5)
    _arrow(c, C + (0.8 if d == "right" else -0.8 if d == "left" else 0),
           C, d, 6.0, 0)
    return c


def g_view():
    c = Canvas()
    c.disc(C, C, 13.5)
    c.rect(8, 8, 19, 19, 0)
    c.rect(10, 10, 17, 17, 1)
    c.rect(12, 12, 23, 23, 0)
    c.rect(14, 14, 21, 21, 1)
    c.rect(12, 12, 17, 17, 0)
    c.rect(14, 14, 17, 17, 1)
    return c


def g_menu():
    c = Canvas()
    c.disc(C, C, 13.5)
    for y in (9, 15, 21):
        c.rect(9, y, 22, y + 1, 0)
    return c


def g_share():
    c = Canvas()
    c.disc(C, C, 13.5)
    # tray
    c.rect(8, 15, 9, 23, 0)
    c.rect(22, 15, 23, 23, 0)
    c.rect(8, 22, 23, 23, 0)
    # arrow up
    c.rect(15, 10, 16, 19, 0)
    c.poly([(C, 5.5), (21.5, 11.5), (10.5, 11.5)], 0)
    return c


def g_capture():
    c = Canvas()
    c.rrect(3, 3, 28, 28, 4)
    c.disc(C, C, 7.5, 0)
    c.disc(C, C, 4.5, 1)
    return c


def g_touchpad():
    c = Canvas()
    c.rrect(1, 6, 30, 25, 3)
    c.rrect(3, 8, 28, 23, 2, 0)
    c.rrect(5, 10, 26, 21, 1, 1)
    # finger tap
    c.disc(C, C, 4.0, 0)
    c.disc(C, C, 2.0, 1)
    return c


def g_xbox_black_white(white):
    c = Canvas()
    c.disc(C, C, 12.5)
    if white:
        c.disc(C, C, 9.5, 0)
    return c


def g_c_button(d, r=12.5):
    c = Canvas()
    c.disc(C, C, r)
    _arrow(c, C, C, d, 6.0, 0)
    return c


def g_c_cluster():
    c = Canvas()
    for d, (x, y) in {"up": (16, 6), "down": (16, 26),
                      "left": (6, 16), "right": (26, 16)}.items():
        c.disc(x, y, 6.0)
        _arrow(c, x, y, d, 3.0, 0)
    return c


# ---------------------------------------------------------------------------
# per-pad definitions

def dpad_set(style):
    f = g_dpad_cross if style == "cross" else g_dpad_sony
    return {
        "dpad": f(None),
        "dpad_up": f({"up"}),
        "dpad_down": f({"down"}),
        "dpad_left": f({"left"}),
        "dpad_right": f({"right"}),
        "dpad_updown": f({"up", "down"}),
        "dpad_leftright": f({"left", "right"}),
    }


def sticks(lbl_l="L", lbl_r="R", press=True):
    g = {"leftstick": g_stick(lbl_l), "rightstick": g_stick(lbl_r)}
    if press:
        g["leftstick_press"] = g_stick(lbl_l, True)
        g["rightstick_press"] = g_stick(lbl_r, True)
    return g


def nintendo_face(two_only=False):
    g = {"south": g_letter_button("B"), "east": g_letter_button("A")}
    if not two_only:
        g["west"] = g_letter_button("Y")
        g["north"] = g_letter_button("X")
    return g


def xbox_face():
    return {"south": g_letter_button("A"), "east": g_letter_button("B"),
            "west": g_letter_button("X"), "north": g_letter_button("Y")}


def sony_face():
    return {"south": g_sony("cross"), "east": g_sony("circle"),
            "west": g_sony("square"), "north": g_sony("triangle")}


def sony_common():
    g = sony_face()
    g.update(dpad_set("sony"))
    g.update(sticks())
    g["leftshoulder"] = g_bumper("L1")
    g["rightshoulder"] = g_bumper("R1")
    g["lefttrigger"] = g_trigger("L2")
    g["righttrigger"] = g_trigger("R2")
    return g


def xbox_modern():
    g = xbox_face()
    g.update(dpad_set("cross"))
    g.update(sticks())
    g["leftshoulder"] = g_bumper("LB")
    g["rightshoulder"] = g_bumper("RB")
    g["lefttrigger"] = g_trigger("LT")
    g["righttrigger"] = g_trigger("RT")
    g["guide"] = g_home()
    return g


def nintendo_modern(bare_plusminus, stick_press, capture):
    g = nintendo_face()
    g.update(dpad_set("cross"))
    g.update(sticks(press=stick_press))
    g["leftshoulder"] = g_bumper("L")
    g["rightshoulder"] = g_bumper("R")
    g["lefttrigger"] = g_trigger("ZL")
    g["righttrigger"] = g_trigger("ZR")
    g["back"] = g_plusminus("-", not bare_plusminus)
    g["start"] = g_plusminus("+", not bare_plusminus)
    g["guide"] = g_home()
    if capture:
        g["capture"] = g_capture()
    return g


def build_pads():
    pads = {}

    nes = nintendo_face(two_only=True)
    nes.update(dpad_set("cross"))
    nes["back"] = g_pill("SELECT")
    nes["start"] = g_pill("START")
    pads["nes"] = nes

    gb = dict(nes)
    pads["gb"] = gb

    gba = dict(nes)
    gba["leftshoulder"] = g_bumper("L")
    gba["rightshoulder"] = g_bumper("R")
    pads["gba"] = gba

    snes = nintendo_face()
    snes.update(dpad_set("cross"))
    snes["leftshoulder"] = g_bumper("L")
    snes["rightshoulder"] = g_bumper("R")
    snes["back"] = g_pill("SELECT")
    snes["start"] = g_pill("START")
    pads["snes"] = snes

    n64 = {"south": g_letter_button("A"), "west": g_letter_button("B")}
    n64.update(dpad_set("cross"))
    n64["leftstick"] = g_stick("")
    n64["rightstick"] = g_c_cluster()
    n64["c_up"] = g_c_button("up")
    n64["c_down"] = g_c_button("down")
    n64["c_left"] = g_c_button("left")
    n64["c_right"] = g_c_button("right")
    n64["leftshoulder"] = g_bumper("L")
    n64["rightshoulder"] = g_bumper("R")
    n64["lefttrigger"] = g_trigger("Z")
    n64["start"] = g_pill("START")
    pads["n64"] = n64

    gc = {"south": g_letter_button("A"), "west": g_letter_button("B", 10.5),
          "east": g_bean("X", False), "north": g_bean("Y", True)}
    gc.update(dpad_set("cross"))
    gc["leftstick"] = g_stick("")
    gc["rightstick"] = g_stick("C", thumb=7.5)
    gc["rightshoulder"] = g_bumper("Z")
    gc["lefttrigger"] = g_trigger("L")
    gc["righttrigger"] = g_trigger("R")
    gc["start"] = g_pill("START")
    pads["gc"] = gc

    pads["wii"] = nintendo_modern(False, False, False)
    pads["wiiu"] = nintendo_modern(False, True, False)
    pads["switch"] = nintendo_modern(True, True, True)

    for name in ("ps1", "ps2", "ps3"):
        g = sony_common()
        g["back"] = g_pill("SELECT")
        g["start"] = g_pill("START")
        if name == "ps3":
            g["guide"] = g_home()
        pads[name] = g

    ps4 = sony_common()
    ps4["back"] = g_pill("SHARE")
    ps4["start"] = g_pill("OPTIONS")
    ps4["guide"] = g_home()
    ps4["touchpad"] = g_touchpad()
    pads["ps4"] = ps4

    ps5 = dict(ps4)
    ps5["back"] = g_pill("CREATE")
    pads["ps5"] = ps5

    xbox = xbox_face()
    xbox.update(dpad_set("cross"))
    xbox.update(sticks())
    xbox["lefttrigger"] = g_trigger("L")
    xbox["righttrigger"] = g_trigger("R")
    xbox["white"] = g_xbox_black_white(True)
    xbox["black"] = g_xbox_black_white(False)
    xbox["leftshoulder"] = xbox["white"]
    xbox["rightshoulder"] = xbox["black"]
    xbox["back"] = g_pill("BACK")
    xbox["start"] = g_pill("START")
    pads["xbox"] = xbox

    x360 = xbox_modern()
    x360["back"] = g_disc_arrow("left")
    x360["start"] = g_disc_arrow("right")
    pads["xbox360"] = x360

    xone = xbox_modern()
    xone["back"] = g_view()
    xone["start"] = g_menu()
    pads["xboxone"] = xone

    xs = dict(xone)
    xs["share"] = g_share()
    pads["xboxseries"] = xs

    return pads


PAD_ORDER = ["nes", "snes", "n64", "gc", "gb", "gba", "wii", "wiiu", "switch",
             "ps1", "ps2", "ps3", "ps4", "ps5",
             "xbox", "xbox360", "xboxone", "xboxseries"]

BUTTON_ORDER = ["south", "east", "west", "north",
                "dpad", "dpad_up", "dpad_down", "dpad_left", "dpad_right",
                "dpad_updown", "dpad_leftright",
                "leftshoulder", "rightshoulder", "lefttrigger", "righttrigger",
                "leftstick", "rightstick", "leftstick_press",
                "rightstick_press",
                "back", "start", "guide",
                "touchpad", "capture", "share", "white", "black",
                "c_up", "c_down", "c_left", "c_right"]


# ---------------------------------------------------------------------------
# contact sheet (preview only)

def write_sheet(pads, path, scale=3):
    cell = SIZE * scale + 8
    label_w = 60
    cols = len(BUTTON_ORDER)
    w = label_w + cols * cell
    h = 14 + len(PAD_ORDER) * cell
    img = [[(40, 40, 48)] * w for _ in range(h)]

    def put(x, y, col):
        if 0 <= x < w and 0 <= y < h:
            img[y][x] = col

    def text(s, x, y, col, sc=2):
        for i, ch in enumerate(s.upper()):
            rows = FONT3x5.get(ch, FONT3x5[" "])
            for ry, row in enumerate(rows):
                for rx, c in enumerate(row):
                    if c == "#":
                        for yy in range(sc):
                            for xx in range(sc):
                                put(x + (i * 4 + rx) * sc + xx,
                                    y + ry * sc + yy, col)

    for ci, b in enumerate(BUTTON_ORDER):
        text(str(ci + 1), label_w + ci * cell + 4, 2, (150, 150, 160))
    for ri, pad in enumerate(PAD_ORDER):
        y0 = 14 + ri * cell
        text(pad, 2, y0 + cell // 2 - 5, (200, 200, 210), 2 if len(pad) < 7 else 1)
        for ci, b in enumerate(BUTTON_ORDER):
            x0 = label_w + ci * cell + 4
            g = pads[pad].get(b)
            for y in range(SIZE * scale):
                for x in range(SIZE * scale):
                    chk = ((x // (4 * scale)) + (y // (4 * scale))) % 2
                    bg = (70, 70, 80) if chk else (60, 60, 70)
                    col = bg
                    if g is not None and g.p[y // scale][x // scale]:
                        col = (240, 220, 120) if ri % 2 else (130, 210, 240)
                    if g is None:
                        col = (45, 45, 52)
                    put(x0 + x, y0 + y, col)
    rows = []
    for y in range(h):
        row = []
        for x in range(w):
            r, g, b = img[y][x]
            row += [r, g, b, 255]
        rows.append(row)
    with open(path, "wb") as f:
        f.write(png_bytes(w, h, rows))


README = """# Gamepad glyphs

32x32 pixel-art button glyphs for the big picture GUI. Generated by
`tools/make_glyphs.py` -- edit the script and re-run it rather than editing
the PNGs.

Every glyph is pure white with a hard (0/255) alpha mask, so tint it with the
theme colour. Labels are cut out of the shapes. Scale with nearest-neighbour
filtering (32 -> 16 for small UI, or integer upscales).

Layout: `<pad>/<button>.png`. Button names follow SDL3 gamepad naming, with
face buttons named by position so the GUI can ask for "south" and get the
right label on every pad (A on Xbox, B on Nintendo, cross on PlayStation).
A pad simply has no file for a button it lacks.

| file | meaning |
|---|---|
| south / east / west / north | face buttons by position |
| dpad, dpad_up/down/left/right, dpad_updown, dpad_leftright | d-pad, whole or highlighted arms |
| leftshoulder / rightshoulder | L / R, L1 / R1, LB / RB (GameCube: Z on the right) |
| lefttrigger / righttrigger | ZL / ZR, L2 / R2, LT / RT (N64: Z) |
| leftstick / rightstick | analog sticks (N64 right = C buttons, GameCube right = C stick) |
| leftstick_press / rightstick_press | stick click (L3 / R3) |
| back / start | select/start, -/+, share/options, view/menu ... |
| guide | home / PS / Xbox button (drawn as a generic house) |
| touchpad | PS4 / PS5 touchpad |
| capture | Switch capture |
| share | Xbox Series share |
| white / black | original Xbox (also copied to leftshoulder/rightshoulder) |
| c_up / c_down / c_left / c_right | N64 C buttons |

Pads: nes snes n64 gc gb gba wii (classic controller) wiiu switch
ps1 ps2 ps3 ps4 ps5 xbox xbox360 xboxone xboxseries.
"""


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    out = os.path.join(here, "..", "assets", "glyphs")
    sheet = None
    args = sys.argv[1:]
    while args:
        a = args.pop(0)
        if a == "--sheet":
            sheet = args.pop(0)
        else:
            out = a
    pads = build_pads()
    count = 0
    for pad in PAD_ORDER:
        d = os.path.join(out, pad)
        os.makedirs(d, exist_ok=True)
        for name, g in pads[pad].items():
            assert name in BUTTON_ORDER, name
            save_canvas(g, os.path.join(d, name + ".png"))
            count += 1
    with open(os.path.join(out, "README.md"), "w") as f:
        f.write(README)
    print("wrote %d glyphs to %s" % (count, os.path.normpath(out)))
    if sheet:
        write_sheet(pads, sheet)
        print("wrote sheet", sheet)


if __name__ == "__main__":
    main()
