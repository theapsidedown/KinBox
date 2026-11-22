#!/usr/bin/env python3
"""
Converts KinBox art into LVGL 9 image descriptors (RGB565 + 8-bit alpha plane).

Inputs (all optional - missing or fully transparent files get placeholder art):
  assets/kindrals_front/KIN_001.png ... KIN_010.png   48x48
  assets/kindrals_back/KIN_001.png  ... KIN_010.png   48x48
  assets/items/ITEM_<n>.png  (n = item id, 1..12)      32x32
  assets/types/TYPE_<n>.png  (n = type id, 0..6)       16x16
  assets/egg.png                                       48x48

Outputs:
  sprites/sprite_data.c

Run from the project root:   python3 tools/gen_assets.py
Requires Pillow (pip install pillow).
"""
import os, sys
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
A = lambda *p: os.path.join(ROOT, "assets", *p)

DEFINED_SPECIES = {0: 0, 1: 1, 2: 4}      # species index -> type index (Ignis, Aqua, Vita)
SPECIES_COUNT = 10
ITEM_COUNT = 13                           # ids 1..12 (0 unused)
TYPE_COUNT = 7
TYPE_COLORS = ["#E8503A", "#3A8FE8", "#B08A4A", "#9AD7E8", "#4CBF5A", "#F2D95C", "#7A4FB5"]
TYPE_LETTERS = "IATEVLN"
ITEM_NAMES = ["", "Sunberry", "Moon Tonic", "Power Seed", "Guard Pebble", "Swift Feather",
              "Honey Bun", "Plush Toy", "Shiny Gem", "Elixir", "Growth Candy", "Stale Crust", "Old Boot"]
ITEM_COLORS = ["", "#E8503A", "#4D79E8", "#E8A23A", "#8A8A9A", "#8ACBE8", "#E8C25C", "#E87AB5",
               "#5CE8D6", "#B05CE8", "#E85CB0", "#8A7A5A", "#6A5A3A"]


def font(size):
    try:
        return ImageFont.load_default(size=size)
    except TypeError:
        return ImageFont.load_default()


def is_blank(im):
    return im is None or im.getchannel("A").getextrema()[1] == 0


def load(path, size):
    if not os.path.exists(path):
        return None
    im = Image.open(path).convert("RGBA")
    if im.size != (size, size):
        print(f"  ! {path} is {im.size}, resizing to {size}x{size}")
        im = im.resize((size, size), Image.NEAREST)
    return None if is_blank(im) else im


def shade(hexcol, f):
    h = hexcol.lstrip("#")
    r, g, b = (int(h[i:i + 2], 16) for i in (0, 2, 4))
    return (min(255, int(r * f)), min(255, int(g * f)), min(255, int(b * f)), 255)


def blob(kind, type_idx, back):
    im = Image.new("RGBA", (48, 48), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    base = TYPE_COLORS[type_idx]
    body = shade(base, 0.75 if back else 1.0)
    dark = shade(base, 0.5)
    if kind == 0:   # flame tuft
        d.polygon([(24, 2), (17, 18), (31, 18)], fill=shade("#FFB020", 0.8 if back else 1))
    elif kind == 1:  # fin
        d.polygon([(24, 6), (14, 20), (34, 20)], fill=dark)
    elif kind == 2:  # leaf
        d.ellipse((20, 2, 36, 18), fill=shade("#2E9E3A", 0.8 if back else 1))
    d.ellipse((6, 12, 42, 46), fill=body, outline=dark, width=2)
    d.ellipse((3, 24, 12, 36), fill=body, outline=dark)
    d.ellipse((36, 24, 45, 36), fill=body, outline=dark)
    if not back:
        for ex in (16, 28):
            d.ellipse((ex, 22, ex + 8, 30), fill="white")
            d.ellipse((ex + 3, 24, ex + 7, 29), fill="black")
        d.arc((17, 30, 31, 40), 20, 160, fill=dark, width=2)
    else:
        d.arc((12, 18, 36, 34), 200, 340, fill=dark, width=2)
    return im


def missing_sprite():
    im = Image.new("RGBA", (48, 48), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.rounded_rectangle((6, 6, 42, 42), 6, fill=(90, 96, 112, 255), outline=(200, 200, 210, 255), width=2)
    d.text((24, 24), "?", fill="white", font=font(26), anchor="mm")
    return im


def egg_sprite():
    im = Image.new("RGBA", (48, 48), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.ellipse((9, 4, 39, 46), fill=(240, 232, 205, 255), outline=(150, 135, 100, 255), width=2)
    for x, y in ((18, 20), (28, 30), (20, 36), (27, 14)):
        d.ellipse((x, y, x + 5, y + 5), fill=(190, 170, 130, 255))
    return im


def item_icon(i):
    im = Image.new("RGBA", (32, 32), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    c = ITEM_COLORS[i]
    d.rounded_rectangle((3, 3, 28, 28), 7, fill=shade(c, 1.0), outline=shade(c, 0.5), width=2)
    d.text((16, 16), ITEM_NAMES[i][0], fill="white", font=font(16), anchor="mm")
    return im


def badge(t):
    im = Image.new("RGBA", (16, 16), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.ellipse((0, 0, 15, 15), fill=TYPE_COLORS[t], outline=shade(TYPE_COLORS[t], 0.5))
    d.text((8, 8), TYPE_LETTERS[t], fill="black" if t in (3, 5) else "white", font=font(10), anchor="mm")
    return im


def to_lvgl(im):
    w, h = im.size
    px = im.load()
    color, alpha = bytearray(), bytearray()
    for y in range(h):
        for x in range(w):
            r, g, b, a = px[x, y]
            v = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
            color += bytes((v & 0xFF, v >> 8))
            alpha.append(a)
    return w, h, bytes(color + alpha)


out, table = [], {}


def emit(name, im):
    w, h, data = to_lvgl(im)
    out.append(f"static const uint8_t {name}_map[] __attribute__((aligned(4))) = {{")
    for i in range(0, len(data), 24):
        out.append("    " + ", ".join(f"0x{b:02x}" for b in data[i:i + 24]) + ",")
    out.append("};")
    out.append(f"const lv_image_dsc_t {name} = {{")
    out.append(f"    .header = {{ .magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_RGB565A8, "
               f".w = {w}, .h = {h}, .stride = {w * 2} }},")
    out.append(f"    .data_size = sizeof({name}_map),")
    out.append(f"    .data = {name}_map,")
    out.append("};\n")
    return name


def main():
    print("Generating sprites...")
    miss = emit("spr_missing", missing_sprite())
    emit("spr_egg", load(A("egg.png"), 48) or egg_sprite())

    front, back = {}, {}
    for s in range(SPECIES_COUNT):
        n = f"KIN_{s + 1:03d}.png"
        f = load(A("kindrals_front", n), 48)
        b = load(A("kindrals_back", n), 48)
        if s in DEFINED_SPECIES:
            t = DEFINED_SPECIES[s]
            if f is None:
                print(f"  placeholder front for species {s + 1}")
                f = blob(s, t, False)
            if b is None:
                print(f"  placeholder back  for species {s + 1}")
                b = blob(s, t, True)
            front[s] = emit(f"spr_front_{s}", f)
            back[s] = emit(f"spr_back_{s}", b)
        elif f is not None or b is not None:   # art exists for a species not in the table yet
            front[s] = emit(f"spr_front_{s}", f or missing_sprite())
            back[s] = emit(f"spr_back_{s}", b or missing_sprite())

    items = {}
    for i in range(1, ITEM_COUNT):
        im = load(A("items", f"ITEM_{i}.png"), 32) or item_icon(i)
        items[i] = emit(f"spr_item_{i}", im)
    badges = {}
    for t in range(TYPE_COUNT):
        im = load(A("types", f"TYPE_{t}.png"), 16) or badge(t)
        badges[t] = emit(f"spr_badge_{t}", im)

    hdr = ["/* GENERATED by tools/gen_assets.py - do not edit by hand. */",
           "#include <lvgl.h>", '#include "sprites.h"', ""]
    decl = [f"extern const lv_image_dsc_t {n};" for n in
            [miss, "spr_egg"] + list(front.values()) + list(back.values()) +
            list(items.values()) + list(badges.values())]
    tbl = ["", f"const lv_image_dsc_t *const spr_front_table[{SPECIES_COUNT}] = {{"]
    tbl += [f"    [{s}] = &{front[s]}," for s in front]
    tbl += ["};", f"const lv_image_dsc_t *const spr_back_table[{SPECIES_COUNT}] = {{"]
    tbl += [f"    [{s}] = &{back[s]}," for s in back]
    tbl += ["};", f"const lv_image_dsc_t *const spr_item_table[{ITEM_COUNT}] = {{"]
    tbl += [f"    [{i}] = &{items[i]}," for i in items]
    tbl += ["};", f"const lv_image_dsc_t *const spr_badge_table[{TYPE_COUNT}] = {{"]
    tbl += [f"    [{t}] = &{badges[t]}," for t in badges]
    tbl += ["};"]
    with open(os.path.join(ROOT, "sprites", "sprite_data.c"), "w") as fh:
        fh.write("\n".join(hdr + decl + [""] + out + tbl) + "\n")
    print("wrote sprites/sprite_data.c")


if __name__ == "__main__":
    sys.exit(main())
