#!/usr/bin/env python3
"""Make Talpy's frames for the display from Talpy Den's own drawing code: the banner's (src/ui/talpy_data.c) or the
weather Talpy's (src/ui/talpy_weather_data.c).

tools/talpy_draw.mjs runs Talpy Den's code (seeded random, fixed clock) and draws every picture of the set as SVG.
This rasterises them with rsvg-convert (the banner at 1.3 px per box unit, the weather Talpy at 1.1), crops every
frame the player shows to one canvas (the union of what they all draw), and stores them as RGB565A8 frames, run-length encoded as talpy_decode reads them
(src/ui/talpy_codec.c), with each mood's rest clip, clips, weights and pauses (src/ui/talpy_set.h). Identical frames
are stored once. The same Talpy Den checkout gives a byte-identical file.

    python3 tools/make_talpy_frames.py ../talpy-den src/ui/talpy_data.c [--sheet PATH] [--pngs DIR]
    python3 tools/make_talpy_frames.py ../talpy-den src/ui/talpy_weather_data.c --set weather [--sheet PATH]

It also writes a contact sheet (every clip a row of its frames) to PATH, by default a temp file,
and prints where. With --pngs DIR (banner only) it also writes transparent PNGs at the board's scale for the layout
spec (design/desk-display-layout.html): each mood's rest frame (rest-<mood>.png), the middle frame of
the sign clip (sign.png) and of the finished reaction (finished.png). The banner fails if its canvas is taller than 112 px or the frames take more than 2,000,000 bytes;
the weather Talpy if his canvas leaves the weather card's 104 x 96 scene box or the frames take more than 800,000
bytes. Needs node, rsvg-convert and Pillow (python3-pillow).
"""
import argparse
import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path

from PIL import Image, ImageDraw

HERE = Path(__file__).resolve().parent
VIEWBOX = (-40, 144)             # the SVGs' viewBox: from (-40, -40), 144 box units square
SHEET_BG = "#171B22"
SHEET_PER_LINE = 16              # frames on one line of the contact sheet

# The sets: what talpy_draw.mjs draws for each, and what the C file is called and checked against
SETS = {
    "banner": {
        "title": "talpy_data.c: Talpy's frames for the display",
        "name": "TALPY_SET",
        "enum": "TALPY_",
        "moods": ("loading", "needs", "working", "done"),   # talpy_mood_t order
        "scale": 1.3,            # px per box unit
        "max_h": 112,            # the banner is 116 px with a 2 px border
        "max_bytes": 2_000_000,
    },
    "weather": {
        "title": "talpy_weather_data.c: the weather Talpy's frames for the display",
        "name": "TALPY_WEATHER_SET",
        "enum": "TW_",
        "moods": ("clear_day", "partly_day", "overcast", "fog", "drizzle", "rain", "snow", "thunder",
                  "clear_night", "partly_night", "plain", "hidden"),   # tw_mood_t order
        "scale": 1.1,            # the mockups' (smaller than the banner's: the sky has room around him)
        # the weather card's scene box, 104 x 96 px, is the mockups' viewBox -14 -22 95 88 (box units): the canvas
        # must sit inside it
        "box": (104, 96),
        "box_origin": (-14, -22),
        "max_bytes": 800_000,
    },
}

OP_TRANSPARENT_MAX = 128         # 0x00..0x7F: op + 1 transparent pixels
OP_REPEAT = 0x80                 # 0x80..0xBF: op - 0x80 + 1 copies of one pixel
OP_LITERAL = 0xC0                # 0xC0..0xFF: op - 0xC0 + 1 literal pixels
OP_RUN_MAX = 64


def encode_frame(pixels):
    """Run-length encodes (rgb565, alpha) pixels as talpy_decode reads them. Alpha 0 is transparent
    (its RGB dropped); 2 or more equal pixels in a row are a repeat, any others literals."""
    px = [(c, a) if a else (0, 0) for c, a in pixels]
    out = bytearray()
    i, n = 0, len(px)
    while i < n:
        j = i
        if px[i][1] == 0:
            while j < n and px[j][1] == 0 and j - i < OP_TRANSPARENT_MAX:
                j += 1
            out.append(j - i - 1)
        else:
            while j < n and px[j] == px[i] and j - i < OP_RUN_MAX:
                j += 1
            if j - i >= 2:
                out.append(OP_REPEAT + j - i - 1)
                out += pixel_bytes(px[i])
            else:  # literals, up to a transparent pixel or the start of a repeat
                while j < n and px[j][1] and j - i < OP_RUN_MAX and not (j + 1 < n and px[j + 1] == px[j]):
                    j += 1
                out.append(OP_LITERAL + j - i - 1)
                for p in px[i:j]:
                    out += pixel_bytes(p)
        i = j
    return bytes(out)


def pixel_bytes(p):
    return bytes((p[0] & 0xFF, p[0] >> 8, p[1]))


def rgb565a8(pixels):
    """The frame as LVGL's RGB565A8: the RGB565 plane (little-endian), then the alpha plane."""
    rgb = bytearray()
    for c, _ in pixels:
        rgb += bytes((c & 0xFF, c >> 8))
    return bytes(rgb) + bytes(a for _, a in pixels)


def fnv1a(data):
    h = 0x811C9DC5
    for b in data:
        h = ((h ^ b) * 0x01000193) & 0xFFFFFFFF
    return h


def to_pixels(im):
    """An RGBA image as (rgb565, alpha) pixels, row by row; a transparent one is (0, 0)."""
    raw = im.tobytes()
    return [((r >> 3) << 11 | (g >> 2) << 5 | b >> 3, a) if a else (0, 0)
            for r, g, b, a in zip(raw[0::4], raw[1::4], raw[2::4], raw[3::4])]


def rasterise(svg, size):
    png = svg.with_suffix(".png")
    subprocess.run(["rsvg-convert", "-w", str(size), "-h", str(size), "-o", str(png), str(svg)], check=True)
    with Image.open(png) as im:
        im = im.convert("RGBA")
    box = im.getchannel("A").getbbox()
    if box and (box[0] == 0 or box[1] == 0 or box[2] == size or box[3] == size):
        sys.exit(f"{svg.name}: the drawing reaches the edge of the SVG's viewBox (it would be cut off)")
    return im, box


def union(boxes):
    boxes = [b for b in boxes if b]
    return (min(b[0] for b in boxes), min(b[1] for b in boxes), max(b[2] for b in boxes), max(b[3] for b in boxes))


def contact_sheet(rows, w, h, path):
    """rows: (label, [image]), none bigger than w x h; each clip a row of its frames (wrapped every SHEET_PER_LINE),
    on the banner's dark."""
    label_h, gap = 14, 4
    cell_w, cell_h = w + gap, h + gap
    lines = [(label, ims[k:k + SHEET_PER_LINE]) for label, ims in rows for k in range(0, len(ims), SHEET_PER_LINE)]
    size = (gap + SHEET_PER_LINE * cell_w, gap + len(rows) * label_h + len(lines) * cell_h)
    sheet = Image.new("RGBA", size, SHEET_BG)
    draw = ImageDraw.Draw(sheet)
    y, last = gap, None
    for label, ims in lines:
        if label != last:
            draw.text((gap, y), label, fill="#9AA3AE")
            y += label_h
            last = label
        for k, im in enumerate(ims):
            x = gap + k * cell_w
            draw.rectangle((x - 1, y - 1, x + im.width, y + im.height), outline="#2A303A")
            sheet.alpha_composite(im, (x, y))
        y += cell_h
    sheet.convert("RGB").save(path)


def write_pngs(folder, clips, crops):
    """PNGs for the layout spec: the rest frame of each mood, the middle frame of the sign clip and of the
    finished reaction (confetti in the air)."""
    folder.mkdir(parents=True, exist_ok=True)

    def middle(clip):
        return crops[clip["frames"][len(clip["frames"]) // 2]["file"]]

    for c in clips:
        if c["kind"] == "rest":
            crops[c["frames"][0]["file"]].save(folder / f"rest-{c['mood']}.png")
        elif c["kind"] == "reaction":
            middle(c).save(folder / "finished.png")
        elif c["name"] == "sign":
            middle(c).save(folder / "sign.png")


def c_file(cfg, commit, data, frames, steps, clips, picks, moods, react, w, h, x, y, frame_names):
    out = [f"""/* {cfg['title']} (talpy_set.h), drawn by Talpy Den's own code.
 * Generated by tools/make_talpy_frames.py from Talpy Den commit {commit}: do not edit, make it again. */
#include "talpy_set.h"

static const uint8_t DATA[] = {{"""]
    for i in range(0, len(data), 20):
        out.append("    " + ", ".join(f"0x{b:02x}" for b in data[i:i + 20]) + ",")
    out.append("};\n\nstatic const talpy_frame_t FRAMES[] = {")
    for i, (offset, size, fw, fh, check) in enumerate(frames):
        out.append(f"    {{{offset}, {size}, {fw}, {fh}, 0x{check:08x}u}}, /* {i}: {frame_names[i]} */")
    out.append("};\n\nstatic const talpy_step_t STEPS[] = {")
    for name, first, count in clips:
        out.append(f"    /* {name} */")
        part = steps[first:first + count]
        for k in range(0, len(part), 10):
            out.append("    " + " ".join(f"{{{f}, {ms}}}," for f, ms in part[k:k + 10]))
    out.append("};\n\nstatic const talpy_clip_t CLIPS[] = {")
    for i, (name, first, count) in enumerate(clips):
        out.append(f"    {{{first}, {count}}}, /* {i}: {name} */")
    out.append("};\n\nstatic const talpy_pick_t PICKS[] = {")
    for clip, weight in picks:
        out.append(f"    {{{clip}, {weight}}}, /* {clips[clip][0]} */")
    out.append("};\n\nstatic const talpy_mood_def_t MOODS[] = {")
    for mood in cfg["moods"]:
        m = moods[mood]
        out.append(f"    [{cfg['enum']}{mood.upper()}] = {{.rest_clip = {m['rest']}, .first_pick = {m['first_pick']}, "
                   f".picks = {m['picks']}, .pause_min_ms = {m['pause'][0]}, .pause_max_ms = {m['pause'][1]}}},")
    out.append(f"""}};

const talpy_set_t {cfg['name']} = {{
    .data = DATA,
    .data_size = {len(data)},
    .w = {w},
    .h = {h},
    .frames = FRAMES,
    .frame_count = {len(frames)},
    .steps = STEPS,
    .step_count = {len(steps)},
    .clips = CLIPS,
    .clip_count = {len(clips)},
    .picks = PICKS,
    .pick_count = {len(picks)},
    .moods = MOODS,
    .mood_count = {len(cfg['moods'])},
    .react_clip = {react},
    .x = {x},
    .y = {y},
}};
""")
    return "\n".join(out)


def placement(cfg, box):
    """Where the canvas (box: its place in the rasterised SVG) sits in the set's scene box, as (x, y) px: (0, 0) for
    the banner; for the weather Talpy, from the scene box's top-left (in box units) at the set's scale."""
    if "box" not in cfg:
        return 0, 0
    ox, oy = ((o - VIEWBOX[0]) * cfg["scale"] for o in cfg["box_origin"])
    return round(box[0] - ox), round(box[1] - oy)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("talpy_den", help="a Talpy Den checkout")
    ap.add_argument("out", help="the C file to write (src/ui/talpy_data.c, src/ui/talpy_weather_data.c)")
    ap.add_argument("--set", choices=sorted(SETS), default="banner", help="which pictures (default: banner)")
    ap.add_argument("--pngs", help="banner: a folder for transparent PNGs: the rest frames, a sign frame, a finished frame")
    ap.add_argument("--sheet", help="where the contact sheet goes (default: a temp file)")
    args = ap.parse_args()
    cfg = SETS[args.set]
    if args.pngs and args.set != "banner":
        ap.error("--pngs is for the banner set")
    size = int(VIEWBOX[1] * cfg["scale"])

    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        if subprocess.run(["node", str(HERE / "talpy_draw.mjs"), args.talpy_den, str(tmp), args.set]).returncode:
            sys.exit("tools/talpy_draw.mjs failed (is the first argument a Talpy Den checkout?)")
        manifest = json.loads((tmp / "manifest.json").read_text())
        if sorted(manifest["moods"]) != sorted(cfg["moods"]):
            sys.exit(f"tools/talpy_draw.mjs drew the moods {', '.join(manifest['moods'])}, not {', '.join(cfg['moods'])}")
        clips = manifest["clips"]
        pictures = {f["file"]: rasterise(tmp / f["file"], size) for c in clips for f in c["frames"]}

    box = union(b for _, b in pictures.values())
    w, h = box[2] - box[0], box[3] - box[1]
    x, y = placement(cfg, box)
    crops = {file: im.crop(box) for file, (im, _) in pictures.items()}

    if args.sheet:
        sheet = Path(args.sheet)
    else:
        fd, name = tempfile.mkstemp(prefix="talpy-sheet-", suffix=".png")
        os.close(fd)
        sheet = Path(name)
    contact_sheet([(f"{c['name']} ({c['kind']}, {len(c['frames'])})", [crops[f["file"]] for f in c["frames"]])
                   for c in clips], w, h, sheet)
    if "max_h" in cfg and h > cfg["max_h"]:
        sys.exit(f"the canvas is {w} x {h} px: taller than {cfg['max_h']} (contact sheet: {sheet})")
    if "box" in cfg and not (x >= 0 and y >= 0 and x + w <= cfg["box"][0] and y + h <= cfg["box"][1]):
        sys.exit(f"the canvas is {w} x {h} px at ({x}, {y}): it leaves the {cfg['box'][0]} x {cfg['box'][1]} scene box "
                 f"(contact sheet: {sheet})")

    if args.pngs:
        write_pngs(Path(args.pngs), clips, crops)

    data, frames, frame_names, index = bytearray(), [], [], {}

    def add_frame(im, name):
        pixels = to_pixels(im)
        enc = encode_frame(pixels)
        key = (im.width, im.height, enc)
        if key not in index:
            index[key] = len(frames)
            frames.append((len(data), len(enc), im.width, im.height, fnv1a(rgb565a8(pixels))))
            frame_names.append(name)
            data.extend(enc)
        return index[key]

    steps, clip_rows, moods, picks = [], [], {}, []
    for c in clips:
        first = len(steps)
        for f in c["frames"]:
            steps.append((add_frame(crops[f["file"]], f["file"][:-4]), f["ms"]))
        clip_rows.append((c["name"], first, len(c["frames"])))
    for mood in cfg["moods"]:
        mine = [i for i, c in enumerate(clips) if c["mood"] == mood]
        rest = [i for i in mine if clips[i]["kind"] == "rest"]
        picked = [(i, manifest["moods"][mood]["weights"][clips[i]["name"]]) for i in mine if clips[i]["kind"] == "clip"]
        moods[mood] = {"rest": rest[0], "first_pick": len(picks), "picks": len(picked),
                       "pause": manifest["moods"][mood]["pause"]}
        picks += picked
    react = next(i for i, c in enumerate(clips) if c["kind"] == "reaction")

    if len(data) > cfg["max_bytes"]:
        sys.exit(f"the frames take {len(data)} bytes: more than {cfg['max_bytes']} (contact sheet: {sheet})")
    assert len(frames) < 65536 and len(steps) < 65536 and all(ms < 65536 for _, ms in steps)

    Path(args.out).write_text(c_file(cfg, manifest["talpy_den"], data, frames, steps, clip_rows, picks, moods, react,
                                     w, h, x, y, frame_names))
    where = f" at ({x}, {y}) in the {cfg['box'][0]} x {cfg['box'][1]} scene box" if "box" in cfg else ""
    print(f"{args.out}: {len(frames)} frames ({len(steps)} steps in {len(clip_rows)} clips), canvas {w} x {h} px"
          f"{where} at {cfg['scale']} px per unit, {len(data)} bytes compressed, from Talpy Den {manifest['talpy_den']}")
    print(f"contact sheet: {sheet}")


if __name__ == "__main__":
    main()
