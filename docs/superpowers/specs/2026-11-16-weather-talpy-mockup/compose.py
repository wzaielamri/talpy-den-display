"""Throwaway: sky + Talpy scenes for the weather mockups -> PNGs and an HTML fragment."""
import base64, subprocess, sys
from pathlib import Path

here = Path(__file__).parent
svgdir = here / 'svg'
pngdir = here / 'png'
pngdir.mkdir(exist_ok=True)
out_html = Path(sys.argv[1])

VB = '-14 -22 95 88'   # scene in box units: 104 x 96 px at about 1.1 px per unit

def sun(cx=64, cy=-7, r=7):
    rays = ''.join(
        f'<line x1="{cx + (r + 3) * c:.2f}" y1="{cy + (r + 3) * s:.2f}" x2="{cx + (r + 7.5) * c:.2f}" y2="{cy + (r + 7.5) * s:.2f}"/>'
        for c, s in [(1, 0), (-1, 0), (0, 1), (0, -1), (.707, .707), (-.707, .707), (.707, -.707), (-.707, -.707)])
    return (f'<circle cx="{cx}" cy="{cy}" r="{r + 6}" fill="#FFCF5A" fill-opacity=".18"/>'
            f'<g stroke="#FFCF5A" stroke-width="2.4" stroke-linecap="round">{rays}</g>'
            f'<circle cx="{cx}" cy="{cy}" r="{r}" fill="#FFCF5A"/>')

def cloud(x, y, k=1.0, fill='#eef1f8', edge='#c4cbdc'):
    # bumpy top, flat bottom: puffs sitting on a rounded base
    parts = [(-6, 0.5, 4.2), (0.5, -2.6, 6), (7, 0, 4.4)]
    base = (x - 10.5 * k, y - 0.5 * k, 22 * k, 5.5 * k, 2.75 * k)
    def shape(grow, paint):
        bx, by, bw, bh, br = base
        puffs = ''.join(f'<circle cx="{x + a * k:.2f}" cy="{y + b * k:.2f}" r="{c * k + grow:.2f}" {paint}/>' for a, b, c in parts)
        return puffs + (f'<rect x="{bx - grow:.2f}" y="{by - grow:.2f}" width="{bw + 2 * grow:.2f}" height="{bh + 2 * grow:.2f}" '
                        f'rx="{br + grow:.2f}" {paint}/>')
    return shape(0.7, f'fill="{edge}"') + shape(0, f'fill="{fill}"')

DROP = 'M0-2.8q2.4 3.6 0 5.2q-2.4-1.5 0-5.2z'
def drops(pts, k=1.0):
    return ''.join(f'<path d="{DROP}" transform="translate({x} {y}) scale({k})" fill="#7cc4f2"/>' for x, y in pts)

def flakes(pts):
    return ''.join(f'<circle cx="{x}" cy="{y}" r="{r}" fill="#ffffff"/>' for x, y, r in pts)

STAR = 'M0-3.4L1-1 3.4 0 1 1 0 3.4-1 1-3.4 0-1-1Z'
def stars(pts):
    return ''.join(f'<path d="{STAR}" transform="translate({x} {y}) scale({k})" fill="#E9EDF2" fill-opacity="{o}"/>' for x, y, k, o in pts)

def moon(cx=62, cy=-6):
    return (f'<circle cx="{cx}" cy="{cy}" r="15" fill="#E6E0C8" fill-opacity=".12"/>'
            f'<circle cx="{cx}" cy="{cy}" r="10.5" fill="#E6E0C8"/><circle cx="{cx + 5.5}" cy="{cy - 4.5}" r="9" fill="#171B22"/>')

def zs():
    return ('<g fill="#8C96A3" font-family="sans-serif" font-weight="bold">'
            '<text x="48" y="22" font-size="6">z</text><text x="53" y="15" font-size="7.5" fill-opacity=".8">z</text></g>')

FOG = ('<g fill="#C9D2DD" fill-opacity=".42"><rect x="24" y="-16" width="62" height="7" rx="3.5"/>'
       '<rect x="-16" y="-2" width="52" height="7" rx="3.5"/>'
       '<rect x="-16" y="24" width="64" height="7" rx="3.5"/><rect x="20" y="37" width="70" height="7" rx="3.5"/>'
       '<rect x="-16" y="50" width="56" height="7" rx="3.5"/></g>')
BEAM = ('<polygon points="35,10 82,-8 82,28" fill="#ffd86b" fill-opacity=".12"/>'
        '<circle cx="35" cy="10" r="8" fill="#ffd86b" fill-opacity=".3"/>')
BOLT = '<path d="M64 0 L54 20 L63 20 L56 38 L75 11 L66 11 L73 0 Z" fill="#FFCF5A"/>'
GREY, GREY_EDGE = '#C9D2DD', '#9aa5b5'

SCENES = [
    ('1', 'Clear (day)', 'sunny', sun(62, -6, 10.5), '', 'Sunglasses on, basking. Rays turn slowly, the glasses glint, he sways a little.'),
    ('2', 'Partly cloudy (day)', 'partly', sun(66, -10, 9) + cloud(55, 3, 1.55), '',
     'Sunglasses on; a cloud drifts over the sun and away again.'),
    ('3', 'Overcast', 'overcast', cloud(10, -11, 1.45, GREY, GREY_EDGE) + cloud(60, -4, 1.7, GREY, GREY_EDGE), '',
     'A warm mug on a grey day (Talpy Den\'s own mug, steaming). Clouds drift; he sips now and then.'),
    ('4', 'Fog', 'fog', BEAM, FOG, 'Squinting; fog bands drift across, his antenna light glows like a fog lamp.'),
    ('5', 'Drizzle', 'drizzle-hat', cloud(60, -8, 1.55), drops([(52, 12), (66, 18), (78, 10), (72, 34), (-6, 14), (80, 46)], 1.2),
     'His yellow rain hat; light drops land on the brim and roll off.'),
    ('6', 'Rain', 'rain', cloud(62, -10, 1.55),
     drops([(54, 10), (68, 14), (80, 8), (62, 30), (76, 32), (82, 50), (-8, 28), (-10, 46), (60, 48)], 1.3),
     'Under his umbrella, held in his paw. Rain falls and bounces off; now and then he twirls it.'),
    ('7', 'Snow', 'snow', flakes([(52, -14, 2.2), (68, -2, 2.6), (80, -16, 2), (58, 14, 2.2), (76, 24, 2.6), (-6, -12, 2.2),
                                  (8, -2, 1.9), (-10, 20, 2.1), (70, 44, 2.1), (30, -16, 2)]), '',
     'Beanie and scarf. Snow falls and settles on the beanie; he shivers, catches a flake.'),
    ('8', 'Thunderstorm', 'thunder', cloud(62, -10, 1.8, '#5D6672', '#454c57') + BOLT, drops([(4, 8), (18, 0), (-6, -6), (30, 6)], 1.2),
     'Ducked into his burrow; at a flash he vanishes, then peeks out again wide-eyed.'),
    ('9', 'Clear (night)', 'night', moon() + stars([(38, -14, 1.2, 1), (80, 12, 0.9, .8), (12, -8, 1, .9), (82, -18, 0.8, .7)]), zs(),
     'Asleep under the moon, no hat. Stars twinkle, a soft "z" floats up now and then.'),
    ('10', 'Partly cloudy (night)', 'night', moon() + cloud(66, 3, 1.35, '#8C96A3', '#6b7480') + stars([(38, -14, 1.2, 1), (12, -8, 1, .9)]), zs(),
     'The same, with a cloud drifting slowly across the moon.'),
]

def scene_svg(talpy, back, front):
    data = base64.b64encode((svgdir / f'{talpy}.svg').read_bytes()).decode()
    return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="{VB}"><rect x="-14" y="-22" width="95" height="88" fill="#171B22"/>'
            f'{back}<image href="data:image/svg+xml;base64,{data}" x="-40" y="-40" width="144" height="144"/>{front}</svg>')

def png(svg_text, name, w, h):
    src = pngdir / f'{name}.svg'
    src.write_text(svg_text)
    dst = pngdir / f'{name}-{w}.png'
    subprocess.run(['rsvg-convert', '-w', str(w), '-h', str(h), '-o', str(dst), str(src)], check=True)
    return 'data:image/png;base64,' + base64.b64encode(dst.read_bytes()).decode()

tiles, strip = [], []
for num, title, talpy, back, front, text in SCENES:
    s = scene_svg(talpy, back, front)
    big = png(s, f'{num}', 208, 192)
    small = png(s, f'{num}', 104, 96)
    tiles.append(f'<div class="tile" data-choice="{num}" onclick="toggleSelect(this)"><img class="big" src="{big}"><h3>{num} · {title}</h3><p>{text}</p></div>')
    strip.append(f'<img src="{small}" title="{num}">')

html = f'''<style>
  .grid {{ display: grid; grid-template-columns: repeat(auto-fill, minmax(230px, 1fr)); gap: 18px; margin-top: 12px; }}
  .tile {{ background: #0E1116; border-radius: 14px; padding: 10px; cursor: pointer; border: 2px solid transparent; }}
  .tile.selected {{ border-color: #4C9BFF; }}
  .tile img.big {{ width: 208px; height: 192px; border-radius: 12px; display: block; margin: 0 auto; }}
  .tile h3 {{ margin: 10px 0 4px; font-size: 15px; color: #E9EDF2; }}
  .tile p {{ margin: 0; font-size: 13px; color: #8C96A3; line-height: 1.4; }}
  .strip {{ display: flex; gap: 8px; flex-wrap: wrap; margin-top: 12px; }}
  .strip img {{ width: 104px; height: 96px; border-radius: 8px; }}
</style>
<h2>Round 3: a bigger sky, spotted at a glance</h2>
<p class="subtitle">Talpy and his props are now drawn the way Talpy Den draws its props (outlines, shading, held in his paw); only the sky is still a quick sketch. Click the ones you like, and tell me in the terminal what still looks off.</p>
<div class="grid">{''.join(tiles)}</div>
<h3 style="margin-top:22px">At real size (104 × 96)</h3>
<div class="strip">{''.join(strip)}</div>
'''
out_html.write_text(html)
print('wrote', out_html)
