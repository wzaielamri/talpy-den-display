#!/usr/bin/env python3
"""Make Talpy's frames for the display (src/ui/talpy_data.c) from Talpy Den's own drawing code.

tools/talpy_draw.mjs runs Talpy Den's code (seeded random, fixed clock) and draws every picture as
SVG. This rasterises them with rsvg-convert at 1.3 px per box unit, crops every frame the player
shows to one canvas (the union of what they all draw; the clock face's sleeping still is cropped
to itself), and stores them as RGB565A8 frames, run-length encoded as talpy_decode reads them
(src/ui/talpy_codec.c), with each mood's rest clip, clips, weights and pauses (src/ui/talpy_set.h).
Identical frames are stored once. The same Talpy Den checkout gives a byte-identical file.

    python3 tools/make_talpy_frames.py ../talpy-den src/ui/talpy_data.c [--sheet PATH] [--pngs DIR]

It also writes a contact sheet (every clip a row of its frames) to PATH, by default a temp file,
and prints where. With --pngs DIR it also writes transparent PNGs at the board's scale for the layout
spec (design/desk-display-layout.html): each mood's rest frame (rest-<mood>.png), the middle frame of
the sign clip (sign.png) and of the finished reaction (finished.png), and the sleeping still
(sleep-still.png). It fails if the canvas is taller than 112 px or the frames take more than
2,000,000 bytes. Needs node, rsvg-convert and Pillow (python3-pillow).
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
SCALE = 1.3                      # px per box unit, everywhere
SIZE = int(144 * SCALE)          # the SVGs' viewBox is 144 box units square
MAX_H = 112                      # the banner is 116 px with a 2 px border
MAX_BYTES = 2_000_000
MOODS = ("loading", "needs", "working", "done")   # talpy_mood_t order
SHEET_BG = "#171B22"
SHEET_PER_LINE = 16              # frames on one line of the contact sheet

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


def rasterise(svg):
    png = svg.with_suffix(".png")
    subprocess.run(["rsvg-convert", "-w", str(SIZE), "-h", str(SIZE), "-o", str(png), str(svg)], check=True)
    with Image.open(png) as im:
        im = im.convert("RGBA")
    box = im.getchannel("A").getbbox()
    if box and (box[0] == 0 or box[1] == 0 or box[2] == SIZE or box[3] == SIZE):
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


def write_pngs(folder, clips, crops, still):
    """PNGs for the layout spec: the rest frame of each mood, the middle frame of the sign clip and of the
    finished reaction (confetti in the air), and the sleeping still."""
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
    still.save(folder / "sleep-still.png")


def c_file(commit, data, frames, steps, clips, picks, moods, finished, sleep_still, w, h, frame_names):
    out = [f"""/* talpy_data.c: Talpy's frames for the display (talpy_set.h), drawn by Talpy Den's own code.
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
    for mood in MOODS:
        m = moods[mood]
        out.append(f"    [TALPY_{mood.upper()}] = {{.rest_clip = {m['rest']}, .first_pick = {m['first_pick']}, "
                   f".picks = {m['picks']}, .pause_min_ms = {m['pause'][0]}, .pause_max_ms = {m['pause'][1]}}},")
    out.append(f"""}};

const talpy_set_t TALPY_SET = {{
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
    .mood_count = {len(MOODS)},
    .react_clip = {finished},
    .sleep_still = {sleep_still},
    .x = 0,
    .y = 0,
}};
""")
    return "\n".join(out)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("talpy_den", help="a Talpy Den checkout")
    ap.add_argument("out", help="the C file to write (src/ui/talpy_data.c)")
    ap.add_argument("--pngs", help="a folder for transparent PNGs: the rest frames, a sign frame, a finished frame, the sleep still")
    ap.add_argument("--sheet", help="where the contact sheet goes (default: a temp file)")
    args = ap.parse_args()

    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        if subprocess.run(["node", str(HERE / "talpy_draw.mjs"), args.talpy_den, str(tmp)]).returncode:
            sys.exit("tools/talpy_draw.mjs failed (is the first argument a Talpy Den checkout?)")
        manifest = json.loads((tmp / "manifest.json").read_text())
        clips = manifest["clips"]
        pictures = {f["file"]: rasterise(tmp / f["file"]) for c in clips for f in c["frames"]}
        still_im, still_box = rasterise(tmp / manifest["sleep_still"])

    box = union(b for _, b in pictures.values())
    w, h = box[2] - box[0], box[3] - box[1]
    crops = {file: im.crop(box) for file, (im, _) in pictures.items()}
    still = still_im.crop(still_box)

    if args.sheet:
        sheet = Path(args.sheet)
    else:
        fd, name = tempfile.mkstemp(prefix="talpy-sheet-", suffix=".png")
        os.close(fd)
        sheet = Path(name)
    # (the sleep still is the done mood's rest still, so it fits the canvas too)
    contact_sheet([(f"{c['name']} ({c['kind']}, {len(c['frames'])})", [crops[f["file"]] for f in c["frames"]])
                   for c in clips] + [("sleep_still", [still])], w, h, sheet)
    if h > MAX_H:
        sys.exit(f"the canvas is {w} x {h} px: taller than {MAX_H} (contact sheet: {sheet})")

    if args.pngs:
        write_pngs(Path(args.pngs), clips, crops, still)

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
    for mood in MOODS:
        mine = [i for i, c in enumerate(clips) if c["mood"] == mood]
        rest = [i for i in mine if clips[i]["kind"] == "rest"]
        picked = [(i, manifest["moods"][mood]["weights"][clips[i]["name"]]) for i in mine if clips[i]["kind"] == "clip"]
        moods[mood] = {"rest": rest[0], "first_pick": len(picks), "picks": len(picked),
                       "pause": manifest["moods"][mood]["pause"]}
        picks += picked
    finished = next(i for i, c in enumerate(clips) if c["kind"] == "reaction")
    sleep_still = add_frame(still, manifest["sleep_still"][:-4])

    if len(data) > MAX_BYTES:
        sys.exit(f"the frames take {len(data)} bytes: more than {MAX_BYTES} (contact sheet: {sheet})")
    assert len(frames) < 65536 and len(steps) < 65536 and all(ms < 65536 for _, ms in steps)

    Path(args.out).write_text(c_file(manifest["talpy_den"], data, frames, steps, clip_rows, picks, moods, finished,
                                     sleep_still, w, h, frame_names))
    print(f"{args.out}: {len(frames)} frames ({len(steps)} steps in {len(clip_rows)} clips, and the sleep still "
          f"{still.width} x {still.height}), canvas {w} x {h} px, {len(data)} bytes compressed, "
          f"from Talpy Den {manifest['talpy_den']}")
    print(f"contact sheet: {sheet}")


if __name__ == "__main__":
    main()
