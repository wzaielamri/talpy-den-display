#!/usr/bin/env node
// Draws Talpy's pictures for the display as SVG files, with Talpy Den's own drawing code
// (<talpy-den>/package/contents/ui/talpy): each mood's rest pose and clips, the "finished"
// reaction and the clock face's sleeping still, and manifest.json saying what each one is.
// tools/make_talpy_frames.py runs it and turns the SVGs into the board's frames.
//
//   node tools/talpy_draw.mjs <talpy-den> <outdir>
//
// Repeatable: Math.random is a seeded generator, in place before Talpy Den's modules load (so
// they're imported dynamically), and the clock is fixed, so one checkout always draws the same.

import { execFileSync } from 'node:child_process';
import { mkdirSync, writeFileSync } from 'node:fs';
import { join, resolve } from 'node:path';
import { pathToFileURL } from 'node:url';

// mulberry32, seeded: Talpy Den's random picks (step lengths, confetti) come out the same each run
function mulberry32(seed) {
    return () => {
        let t = seed += 0x6D2B79F5;
        t = Math.imul(t ^ t >>> 15, t | 1);
        t ^= t + Math.imul(t ^ t >>> 7, t | 61);
        return ((t ^ t >>> 14) >>> 0) / 4294967296;
    };
}
Math.random = mulberry32(0x7A1B);

const [den, outdir] = process.argv.slice(2);
if (!den || !outdir) {
    console.error('usage: node tools/talpy_draw.mjs <talpy-den> <outdir>');
    process.exit(2);
}
const ui = join(resolve(den), 'package/contents/ui/talpy');
const { Talpy } = await import(pathToFileURL(join(ui, 'talpy.mjs')).href);
const { MOODS } = await import(pathToFileURL(join(ui, 'actions.mjs')).href);

// The board's moods (talpy_mood_t order) and their clips: Talpy Den's quiet moves for each
const CLIPS = { loading: ['lookaround'], needs: ['alert', 'sign', 'wave'], working: ['typing', 'thinking', 'read'],
                done: ['sleep', 'nap'] };
const THEME = { fx: '#8C96A3', text: '#E9EDF2' };   // Talpy Den's fx (z's, notes) and text colours
const VIEWBOX = '-40 -40 144 144';                  // box units: the 64-unit box and room for what sticks out
const START = 1e6;                                  // the clock (ms) each Talpy starts at
const STEP = 100;                                   // a clip's frame step (ms)
const SETTLE_TICKS = 50;
const STILL_MS = 3000;                              // a rest still, before its blink
const BLINK_MS = 110;                               // as long as Talpy Den's blink
const BOUNCE_MS = 420, BOUNCE_FRAMES = 4;           // Talpy Den's bounce period (bodyMove 'bounce'), sampled
const FAR = 1e9;

// ---------------------------------------------------------------- SvgContext
// Copied from Talpy Den's tools/talpy_icons.mjs (same author, MIT License): a stand-in for a canvas
// that records what Talpy draws, every point already moved by the transform, as SVG paths. One
// change: fillText writes SVG text, where the original draws nothing (sleep's and nap's z's are text).
// The z's use the system's sans-serif (rsvg/fontconfig), so the pictures are the same only on the same machine.

const num = v => +v.toFixed(2);

// A color as SVG attributes: Qt's SVG renderer (Plasma's notifications)
// reads no rgba(), it paints that black; so '#rrggbb' and an opacity
// (times alpha: the canvas's globalAlpha)
function paint(attr, color, alpha) {
    const m = /^rgba?\(([^)]*)\)$/.exec(color);
    const v = m ? m[1].split(',').map(Number) : null;
    const hex = v ? '#' + v.slice(0, 3).map(c => Math.round(c).toString(16).padStart(2, '0')).join('') : color;
    const a = (v && v.length > 3 ? v[3] : 1) * (alpha === undefined ? 1 : alpha);
    return ` ${attr === 'stop' ? 'stop-color' : attr}="${hex}"` + (a < 1 ? ` ${attr}-opacity="${num(a)}"` : '');
}

class SvgContext {
    constructor() {
        this.out = [];
        this.defs = [];
        this.ids = 0;
        this.s = { m: [1, 0, 0, 1, 0, 0], alpha: 1, fillStyle: '#000', strokeStyle: '#000', lineWidth: 1,
                   lineCap: 'butt', lineJoin: 'miter', groups: 0, op: 'source-over',
                   font: '10px sans-serif', textAlign: 'start', textBaseline: 'alphabetic' };
        this.stack = [];
        this.path = [];
    }
    save() {
        this.stack.push(Object.assign({}, this.s, { m: this.s.m.slice() }));
        this.s.groups = 0;
    }
    restore() {
        for (let i = 0; i < this.s.groups; i++)
            this.out.push('</g>');
        this.s = this.stack.pop();
    }
    transform(a, b, c, d, e, f) {
        const m = this.s.m;
        this.s.m = [m[0] * a + m[2] * b, m[1] * a + m[3] * b, m[0] * c + m[2] * d, m[1] * c + m[3] * d,
                    m[0] * e + m[2] * f + m[4], m[1] * e + m[3] * f + m[5]];
    }
    translate(x, y) { this.transform(1, 0, 0, 1, x, y); }
    scale(x, y) { this.transform(x, 0, 0, y, 0, 0); }
    rotate(r) { this.transform(Math.cos(r), Math.sin(r), -Math.sin(r), Math.cos(r), 0, 0); }
    pt(x, y) {
        const m = this.s.m;
        return `${num(m[0] * x + m[2] * y + m[4])} ${num(m[1] * x + m[3] * y + m[5])}`;
    }
    set globalAlpha(v) { this.s.alpha = v; }
    get globalAlpha() { return this.s.alpha; }
    set fillStyle(v) { this.s.fillStyle = v; }
    set strokeStyle(v) { this.s.strokeStyle = v; }
    set lineWidth(v) { this.s.lineWidth = v; }
    set lineCap(v) { this.s.lineCap = v; }
    set lineJoin(v) { this.s.lineJoin = v; }
    set globalCompositeOperation(v) { this.s.op = v; }
    set font(v) { this.s.font = v; }
    set textAlign(v) { this.s.textAlign = v; }
    set textBaseline(v) { this.s.textBaseline = v; }
    // (Talpy Den's text is centred both ways: art.text; the canvas's 'middle' is SVG's 'central')
    fillText(text, x, y) {
        const esc = String(text).replace(/&/g, '&amp;').replace(/</g, '&lt;');
        this.out.push(`<text transform="matrix(${this.s.m.map(num).join(' ')})" x="${num(x)}" y="${num(y)}"`
            + ` style="font: ${this.s.font}"` + (this.s.textAlign === 'center' ? ' text-anchor="middle"' : '')
            + (this.s.textBaseline === 'middle' ? ' dominant-baseline="central"' : '')
            + paint('fill', this.s.fillStyle, this.s.alpha) + `>${esc}</text>`);
    }
    beginPath() { this.path = []; }
    moveTo(x, y) { this.path.push('M' + this.pt(x, y)); }
    lineTo(x, y) { this.path.push('L' + this.pt(x, y)); }
    bezierCurveTo(a, b, c, d, e, f) { this.path.push('C' + this.pt(a, b) + ' ' + this.pt(c, d) + ' ' + this.pt(e, f)); }
    closePath() { this.path.push('Z'); }
    rect(x, y, w, h) {
        this.moveTo(x, y);
        this.lineTo(x + w, y);
        this.lineTo(x + w, y + h);
        this.lineTo(x, y + h);
        this.closePath();
    }
    createLinearGradient(x0, y0, x1, y1) {
        const id = 'g' + (++this.ids), ctx = this, a = this.pt(x0, y0).split(' '), b = this.pt(x1, y1).split(' ');
        const stops = [];
        ctx.defs.push(() => `<linearGradient id="${id}" gradientUnits="userSpaceOnUse" x1="${a[0]}" y1="${a[1]}" x2="${b[0]}" y2="${b[1]}">`
            + stops.map(s => `<stop offset="${s[0]}"${paint('stop', s[1])}/>`).join('') + '</linearGradient>');
        return { url: `url(#${id})`, addColorStop: (o, c) => stops.push([o, c]) };
    }
    paint(kind) {
        const style = kind === 'fill' ? this.s.fillStyle : this.s.strokeStyle;
        const color = typeof style === 'object' ? style.url : style;
        if (this.s.op === 'destination-out')
            return this.out.push(`<path d="${this.path.join('')}" fill="${kind === 'fill' ? '#000' : 'none'}"`
                + (kind === 'stroke' ? ` stroke="#000" stroke-width="${num(this.width())}" stroke-linecap="${this.s.lineCap}"` : '')
                + ` data-cut="1"/>`);
        let a = `<path d="${this.path.join('')}"`;
        if (kind === 'fill')
            a += paint('fill', color, this.s.alpha);
        else
            a += ` fill="none"${paint('stroke', color, this.s.alpha)} stroke-width="${num(this.width())}" stroke-linecap="${this.s.lineCap}" stroke-linejoin="${this.s.lineJoin}"`;
        this.out.push(a + '/>');
    }
    width() {
        const m = this.s.m;
        return this.s.lineWidth * Math.sqrt(Math.abs(m[0] * m[3] - m[1] * m[2]));
    }
    fill() { this.paint('fill'); }
    stroke() { this.paint('stroke'); }
    clip() {
        const id = 'c' + (++this.ids), d = this.path.join('');
        this.defs.push(() => `<clipPath id="${id}"><path d="${d}"/></clipPath>`);
        this.out.push(`<g clip-path="url(#${id})">`);
        this.s.groups += 1;
    }
    svg(viewBox, background) {
        // what a cut-out removes is masked out of the rest (the panel icon's eyes)
        const cuts = this.out.filter(l => l.includes('data-cut="1"'));
        const body = this.out.filter(l => !l.includes('data-cut="1"')).join('');
        let content = body;
        if (cuts.length) {
            this.defs.push(() => `<mask id="cut"><rect x="-100" y="-100" width="300" height="300" fill="#fff"/>${cuts.join('').replace(/data-cut="1"/g, '')}</mask>`);
            content = `<g mask="url(#cut)">${body}</g>`;
        }
        return `<svg xmlns="http://www.w3.org/2000/svg" viewBox="${viewBox}">`
            + (this.defs.length ? `<defs>${this.defs.map(f => f()).join('')}</defs>` : '')
            + (background || '') + content + '</svg>\n';
    }
}

// ---------------------------------------------------------------- Talpy's pictures

function picture(t) {
    const ctx = new SvgContext();
    t.draw(ctx);
    return ctx.svg(VIEWBOX);
}

// Talpy in a mood's base pose, settled (what's eased has got there). Its eyes look ahead and don't
// blink meanwhile, as in Talpy Den's own still pictures (Talpy.pose), so every settled Talpy of a
// mood is the same picture: the rest still, and where each of its clips starts.
function settled(mood) {
    const t = new Talpy({ brain: false, playful: false, theme: THEME });
    t.nextBlink = t.nextGlance = START + FAR;
    t.tick(START);
    t.setMood(mood);
    for (let i = 1; i <= SETTLE_TICKS; i++)
        t.tick(START + i * STEP);
    return t;
}

// What the board loops during a mood's pauses: a bouncing base pose (needs) one bounce period;
// any other the settled still, then a blink if its eyes blink
function rest(mood) {
    const t = settled(mood);
    if (t.st.body === 'bounce') {
        const into = (t.now - t.since.body) % BOUNCE_MS, from = t.now + (BOUNCE_MS - into) % BOUNCE_MS;
        const ms = BOUNCE_MS / BOUNCE_FRAMES;
        return Array.from({ length: BOUNCE_FRAMES }, (_, i) => {
            t.tick(from + i * ms);
            return { svg: picture(t), ms };
        });
    }
    const frames = [{ svg: picture(t), ms: STILL_MS }];
    t.nextBlink = t.now;   // Talpy Den's tick blinks now, if these eyes blink (its BLINKING)
    t.tick(t.now);
    if (t.blinkUntil > t.now)
        frames.push({ svg: picture(t), ms: BLINK_MS });
    return frames;
}

// A clip from the settled pose: a picture every STEP while the action runs
function clip(mood, name) {
    const t = settled(mood);
    t.react(name);
    t.nextBlink = t.nextGlance = 0;   // from here it blinks and glances as Talpy Den has it
    const frames = [];
    for (let now = t.now; ; now += STEP) {
        t.tick(now);
        if (!t.run)
            break;
        frames.push({ svg: picture(t), ms: STEP });
    }
    if (!frames.length)
        throw new Error(`Talpy Den has no action "${name}"`);
    return frames;
}

function commit() {
    try {
        return execFileSync('git', ['-C', den, 'rev-parse', '--short=8', 'HEAD'],
                            { encoding: 'utf8', stdio: ['ignore', 'pipe', 'ignore'] }).trim();
    } catch {
        return 'unknown';
    }
}

mkdirSync(outdir, { recursive: true });
const save = (name, svg, i) => {
    const file = `${name}-${String(i).padStart(3, '0')}.svg`;
    writeFileSync(join(outdir, file), svg);
    return file;
};
const clips = [];
const add = (name, mood, kind, frames) =>
    clips.push({ name, mood, kind, frames: frames.map((f, i) => ({ file: save(name, f.svg, i), ms: f.ms })) });

const moods = {};
for (const [mood, names] of Object.entries(CLIPS)) {
    // the weights the board picks by: Talpy Den's quiet ones (Playful Talpy off), or the pool when
    // a mood has no quiet one (loading), as Talpy.think picks
    const pool = MOODS[mood].calm || MOODS[mood].pool;
    if (Object.keys(pool).sort().join() !== names.slice().sort().join())
        throw new Error(`${mood}: Talpy Den's moves are ${Object.keys(pool).join(', ')}, not ${names.join(', ')}`);
    const weights = Object.fromEntries(names.map(n => [n, Math.round(pool[n] * 10)]));   // (the board's are × 10)
    moods[mood] = { pause: MOODS[mood].pause, weights };
    add(`${mood}-rest`, mood, 'rest', rest(mood));
    for (const name of names)
        add(name, mood, 'clip', clip(mood, name));
}
add('finished', 'done', 'reaction', clip('done', 'finished'));
const sleepStill = save('sleep_still', picture(settled('done')), 0);

const manifest = { talpy_den: commit(), moods, clips, sleep_still: sleepStill };
writeFileSync(join(outdir, 'manifest.json'), JSON.stringify(manifest, null, 1) + '\n');
