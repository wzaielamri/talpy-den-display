#!/usr/bin/env node
// Draws Talpy's pictures for the display as SVG files, with Talpy Den's own drawing code
// (<talpy-den>/package/contents/ui/talpy), and manifest.json saying what each one is. Two sets:
// banner: each chat mood's rest pose and clips, the "finished" reaction;
// weather: Talpy for each weather (with a prop: sunglasses, mug, rain hat, umbrella, beanie and scarf), his rest
// and his few moves, and the thunder duck. tools/make_talpy_frames.py runs it and turns the SVGs into the board's frames.
//
//   node tools/talpy_draw.mjs <talpy-den> <outdir> [banner|weather]
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

const [den, outdir, set = 'banner'] = process.argv.slice(2);
if (!den || !outdir || (set !== 'banner' && set !== 'weather')) {
    console.error('usage: node tools/talpy_draw.mjs <talpy-den> <outdir> [banner|weather]');
    process.exit(2);
}
const ui = join(resolve(den), 'package/contents/ui/talpy');
const { Talpy } = await import(pathToFileURL(join(ui, 'talpy.mjs')).href);
const { ACTIONS, MOODS } = await import(pathToFileURL(join(ui, 'actions.mjs')).href);
const art = await import(pathToFileURL(join(ui, 'art.mjs')).href);
const { tracePath, traceRoundRect } = await import(pathToFileURL(join(ui, 'paths.mjs')).href);

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

function picture(t, particles = true) {
    const ctx = new SvgContext();
    t.draw(ctx, !particles);
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
    return stillAndBlink(t, true);
}

// The settled still, then (blinks) a blink if these eyes blink
function stillAndBlink(t, blinks) {
    const frames = [{ svg: picture(t), ms: STILL_MS }];
    if (!blinks)
        return frames;
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
    return record(t, name, true);
}

// A picture every STEP while the action under way runs (particles: with what flies off)
function record(t, name, particles) {
    const frames = [];
    for (let now = t.now; ; now += STEP) {
        t.tick(now);
        if (!t.run)
            break;
        frames.push({ svg: picture(t, particles), ms: STEP });
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

// ================================================================ the banner set

function banner() {
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
    return { moods, clips };
}

// ================================================================ the weather set
// Talpy on the weather card and the clock face: one picture per weather (the board's tw_mood_t), Talpy only (the
// board draws the sky around him), with the props of the approved mockup
// (docs/superpowers/specs/2026-11-16-weather-talpy-mockup/render.mjs) drawn with Talpy Den's own helpers (art), and
// his lamp Talpy Den's calm yellow (idle) in every one, so it never reads as a chat's state.

const WEATHER_PAUSE = [4000, 9000];   // calmer than the banner: several seconds between his moves
const WEATHER_WEIGHT = 10;            // a weather's moves are picked as equals
const STEAM_MS = 2200;                // Talpy Den's mug steam rises once in this long (art.propBack)
const STEAM_FRAMES = 6;
const SWAY_UPRIGHT = 250;             // Talpy Den's body move 'sway' leans ±10° every 500 ms: upright 250 ms in,
const SWAY_MS = 1500;                 // and every half swing after; ours, upright to upright
const GLINT_STEPS = 5;                // a prop's own move: steps from the rest picture round to it again
const TWIRL_STEPS = 8;

// ---------------------------------------------------------------- the props
// In Talpy's torso frame (box units, as Talpy Den draws his face and arms), so they sink, sway and breathe with him.

const HANDLE = '#6b5b3e';   // Talpy Den's magnifier handle
const LENSES = [19.8, 34.2];   // each lens's left edge: 10 × 7.2 units from y 33.4

// Sunglasses. t.glare: a glint sweeping across both lenses (0 to 1 of the way), none below 0
function sunglasses(ctx, t) {
    for (const x of LENSES)
        art.rrect(ctx, x, 33.4, 10, 7.2, 2.8, art.INK);
    art.line(ctx, 'M29.8 35.8h4.4', art.INK, 1.3);
    art.line(ctx, 'M19.9 35.4l-4.4-1.2M44.1 35.4l4.4-1.2', art.INK, 1.1);
    art.line(ctx, 'M21.6 35.4h2.6M36 35.4h2.6', '#ffffff', 0.9, 0.55);
    if (t.glare < 0)
        return;
    ctx.save();
    ctx.beginPath();
    for (const x of LENSES)
        traceRoundRect(ctx, x, 33.4, 10, 7.2, 2.8);
    ctx.clip();
    for (const x of LENSES) {
        const g = x - 3 + 13 * t.glare;   // (a slanted band 3 units across: from just left of the lens to past it)
        art.line(ctx, `M${num(g)} 41.4L${num(g + 3)} 32.6`, '#ffffff', 1.6, 0.75);
    }
    ctx.restore();
}

function rainHat(ctx) {
    const crown = 'M20.5 25C20.5 16 26 12.5 32 12.5S43.5 16 43.5 25Z';
    const brim = 'M13.5 26.5C18 23.4 46 23.4 50.5 26.5C52.6 28.2 50.6 30.6 47 30.2C40 28.6 24 28.6 17 30.2C13.4 30.6 11.4 28.2 13.5 26.5Z';
    art.path(ctx, crown, '#f2c94c');
    art.line(ctx, crown, '#c49a1e', 0.8);
    art.path(ctx, brim, '#f2c94c');
    art.line(ctx, brim, '#c49a1e', 0.8);
    art.line(ctx, 'M21 24.6C26 23.2 38 23.2 43 24.6', '#c49a1e', 0.8);
    art.ellipse(ctx, 26, 17.5, 2.2, 1.2, '#ffffff', 0.4);
}

function beanie(ctx) {
    const crown = 'M19.5 26C19.5 15 25 11 32 11S44.5 15 44.5 26Z';
    art.path(ctx, crown, '#ff6b8b');
    art.line(ctx, crown, '#c23f5c', 0.8);
    art.line(ctx, 'M25 13.5Q24 19 24.5 25M32 11.3V25M39 13.5Q40 19 39.5 25', '#e2506f', 0.7);
    art.rrect(ctx, 18.4, 23.6, 27.2, 5.2, 2.6, '#e9edf2', '#c4cbdc', 0.7);
    art.circle(ctx, 24, 11.6, 3.3, '#e9edf2');
}

function scarf(ctx) {
    const band = 'M13.6 43.6C24 47.2 40 47.2 50.4 43.6L50.6 48.8C40 52.4 24 52.4 13.4 48.8Z';
    art.path(ctx, band, '#ff6b8b');
    art.line(ctx, band, '#c23f5c', 0.8);
    art.line(ctx, 'M20 45.6V50.6M27 46.6V51.6M34 46.8V51.8M41 46.2V51.2', '#ffffff', 0.9, 0.6);
    art.rrect(ctx, 37.5, 47.6, 5.6, 11, 1.8, '#ff6b8b', '#c23f5c', 0.8);
    art.line(ctx, 'M38.3 52.2h4M38.3 55.2h4', '#ffffff', 0.9, 0.6);
}

// The umbrella, in the frame of the arm that holds it up (raised `angle` degrees, as Talpy Den's arms turn): its
// shaft and handle from the paw, its canopy upright over his head. turn: how far round its shaft the canopy has
// turned (0 to 1, once round).
//
// The canopy's darker gores (every other one) and its ribs (one by each dark gore) turn with it: they move along the
// canopy, all from its top, by a pair of gores (24 units) once round, cut to the canopy. Each rib bows out with the
// canopy: straight down the middle, Q7 25 halfway out (the mockup's), along the canopy's edge at ±24 (so the
// gores fill to it); the gores reach below the rim and the canopy's own scallops cut them.
const CANOPY = 'M-24 34C-23 26-13 21 0 21C13 21 23 26 24 34Q18 31 12 34Q6 31 0 34Q-6 31-12 34Q-18 31-24 34Z';
const GORE = 12;   // a gore's width at the rim
function ribBend(x) {   // the control point of the rib from the canopy's top (0 21) to its rim at x, as 'cx cy'
    const out = Math.max(0, Math.abs(x) / GORE - 1);   // (0 up to halfway out, 1 at the edge)
    const cx = out ? Math.sign(x) * (7 + 15.4 * out) : 7 * x / GORE;
    return `${num(cx)} ${num(25 - 4 * out)}`;
}
function umbrella(ctx, angle, turn) {
    art.line(ctx, 'M0 6.5L0 34', HANDLE, 1.5);
    art.line(ctx, 'M0 6.5q0-3.2-2.6-3.2q-2.2 0-2.2 2.2', HANDLE, 1.5);
    ctx.save();
    art.about(ctx, 0, 34, -angle);
    art.path(ctx, CANOPY, '#ff6b8b');
    ctx.save();
    ctx.beginPath();
    tracePath(ctx, CANOPY);
    ctx.clip();
    for (let a = -3 * GORE + 2 * GORE * turn; a < 2 * GORE; a += 2 * GORE) {
        const b = a + GORE;
        art.path(ctx, `M0 21Q${ribBend(a)} ${num(a)} 34V36H${num(b)}V34Q${ribBend(b)} 0 21Z`, '#e2506f');
        art.line(ctx, `M0 21Q${ribBend(a)} ${num(a)} 34`, '#c23f5c', 0.6);
    }
    ctx.restore();
    art.line(ctx, CANOPY, '#c23f5c', 0.8);
    art.ellipse(ctx, -14, 27, 3.4, 1.5, '#ffffff', 0.4);
    art.circle(ctx, 0, 20.2, 1.1, HANDLE);
    ctx.restore();
}

// ---------------------------------------------------------------- Talpy wearing them

// Talpy Den's Talpy with a weather's props: the face props drawn over his face (after drawFace), the umbrella in his
// right arm, raised to the hand's angle whatever his arms do (in place of Talpy Den's drawArm for it)
class WeatherTalpy extends Talpy {
    constructor(look) {
        super({ brain: false, playful: false, theme: THEME });
        this.look = look;          // LOOKS: look.face(ctx, t), look.hand: { angle, draw(ctx, angle, t) }
        this.glare = -1;           // the sunglasses' glint (sunglasses)
        this.canopyTurn = 0;       // the umbrella's (umbrella)
        this.stillClock = null;    // (see drawLamp)
    }

    drawFace(ctx, now, st, v, pal, move, ms) {
        super.drawFace(ctx, now, st, v, pal, move, ms);
        if (this.look.face)
            this.look.face(ctx, this);
    }

    drawArm(ctx, x, angle, dy, pal, holds, now) {
        const hand = this.look.hand;
        if (!hand || x < 32) {   // (drawCreature draws the left arm at x 14.5, the right one at 49.5)
            super.drawArm(ctx, x, angle, dy, pal, holds, now);
            return;
        }
        super.drawArm(ctx, x, hand.angle, dy, pal, holds, now);
        ctx.save();
        ctx.translate(x, 41 + dy);   // (the arm's frame, as Talpy Den's drawArm turns it)
        ctx.rotate(hand.angle * Math.PI / 180);
        hand.draw(ctx, hand.angle, this);
        ctx.restore();
    }

    // With stillClock set, his lamp glows as it did then and he doesn't breathe (drawn unscaled, as at the start of
    // a breath), while the rest goes by his clock: the mug's steam loops by itself, with no jump in them
    drawLamp(ctx, now, st, v, pal, move) {
        super.drawLamp(ctx, this.stillClock === null ? now : this.stillClock, st, v, pal, move);
    }

    drawCreature(ctx, now, st, v, pal, move, ms) {
        if (this.stillClock !== null && !move.torso)
            move = Object.assign({}, move, { torso: true });   // (a torso move with no scale or turn: no breath)
        super.drawCreature(ctx, now, st, v, pal, move, ms);
    }
}

// Talpy in a weather's pose, settled, as settled() for a mood: its pose is his base, as Talpy.setMood makes a mood's
// (setMood takes only Talpy Den's moods), so what an action leaves alone stays as the weather has it
function dressed(look) {
    const t = new WeatherTalpy(look);
    t.nextBlink = t.nextGlance = START + FAR;
    t.tick(START);
    t.base = Object.assign({ lamp: 'idle' }, look.base);
    t.apply({});
    for (let i = 1; i <= SETTLE_TICKS; i++)
        t.tick(START + i * STEP);
    return t;
}

// What the board loops during a weather's pauses: the mug's steam rising (one round of it, all else still); any
// other the settled still, then a blink if his eyes blink and show (behind sunglasses a blink would only flicker
// at the lenses' edge)
function weatherRest(look) {
    const t = dressed(look);
    if (look.steam) {
        const from = t.now;
        t.stillClock = from;
        return Array.from({ length: STEAM_FRAMES }, (_, i) => {
            t.tick(from + i * STEAM_MS / STEAM_FRAMES);
            return { svg: picture(t, false), ms: Math.round(STEAM_MS / STEAM_FRAMES) };
        });
    }
    return stillAndBlink(t, !look.shades);
}

// A move of a prop's own (n steps of STEP, from the rest picture round to it again): set(t, p) puts it p of the way.
// His eyes look ahead and don't blink, as at rest.
function propMove(look, n, set) {
    const t = dressed(look), from = t.now;
    return Array.from({ length: n - 1 }, (_, i) => {
        set(t, (i + 1) / n);
        t.tick(from + (i + 1) * STEP);
        return { svg: picture(t, false), ms: STEP };
    });
}

// One of Talpy Den's actions (start(t) begins it) from the settled pose, blinking and glancing as Talpy Den has it.
// What flies off (particles) only with particles: the board's sky has the weather, and Talpy Den's would double it
// (shiver's snow)
function action(look, name, start, particles = false) {
    const t = dressed(look);
    start(t);
    t.nextBlink = t.nextGlance = 0;
    return record(t, name, particles);
}

// Talpy Den's body move 'sway', upright to upright (its pictures in between: the rest picture is either end)
function sway(look) {
    const t = dressed(look), from = t.now;
    t.play({ steps: () => [{ body: 'sway', t: SWAY_UPRIGHT + SWAY_MS }] }, null, false);
    return Array.from({ length: SWAY_MS / STEP - 1 }, (_, i) => {
        t.tick(from + SWAY_UPRIGHT + (i + 1) * STEP);
        return { svg: picture(t, false), ms: STEP };
    });
}

// The thunder duck (at a flash): right down into his burrow, eyes shut, and back up to peek out, eyes ahead
function duck(look) {
    const t = dressed(look);
    t.play({ steps: () => [{ depth: 'hidden', eyes: 'closed', t: 1000 }, { depth: 'peek', t: 1000 }] }, null, false);
    return record(t, 'duck', false);
}

// Talpy Den's coffee, its sips only: its first step looks at the mug, which his happy eyes (a curve) don't show
const SIPS = { keep: ACTIONS.coffee.keep, steps: w => ACTIONS.coffee.steps(w).filter(s => s.sip !== undefined) };

const MOVES = {
    glint: look => propMove(look, GLINT_STEPS, (t, p) => { t.glare = p; }),
    sway,
    sip: look => action(look, 'coffee', t => t.play(SIPS, null, false)),
    look: look => action(look, 'lookaround', t => t.react('lookaround')),
    twirl: look => propMove(look, TWIRL_STEPS, (t, p) => { t.canopyTurn = p; }),
    shiver: look => action(look, 'shiver', t => t.react('shiver')),
    sleep: look => action(look, 'sleep', t => t.react('sleep'), true),   // with its z's
    duck,
};

// Each weather's picture: Talpy's pose (Talpy Den's pose keys; lamp idle), what he wears on his face (face) or holds
// up in his right paw (hand), his moves (MOVES) and reaction; shades: his eyes are behind sunglasses; steam: his
// rest is the mug's steam rising
const LOOKS = {
    sunny: { base: { eyes: 'open', mouth: 'smile', depth: 'half' }, face: sunglasses, shades: true,
             moves: ['glint', 'sway'] },
    mug: { base: { prop: 'mug', arms: 'hold', depth: 'high', eyes: 'happy', mouth: 'small' }, steam: true, moves: ['sip'] },
    fog: { base: { eyes: 'squint', mouth: 'flat', depth: 'half' }, moves: [] },
    drizzle: { base: { eyes: 'open', mouth: 'smile', depth: 'half' }, face: rainHat, moves: ['look'] },
    rain: { base: { eyes: 'open', mouth: 'smile', depth: 'half' },
            hand: { angle: 160, draw: (ctx, angle, t) => umbrella(ctx, angle, t.canopyTurn) }, moves: ['twirl'] },
    snow: { base: { eyes: 'happy', mouth: 'smile', depth: 'half' }, face: ctx => { beanie(ctx); scarf(ctx); },
            moves: ['shiver'] },
    thunder: { base: { eyes: 'wide', mouth: 'o', depth: 'peek' }, moves: [], reaction: 'duck' },
    night: { base: { eyes: 'closed', mouth: 'small', depth: 'low', plume: 'droop' }, moves: ['sleep'] },
    plain: { base: { eyes: 'open', mouth: 'smile', depth: 'half' }, moves: [] },
    // (only his lamp shows; his eyes closed, so there's no blink to draw that wouldn't show)
    hidden: { base: { eyes: 'closed', mouth: 'small', depth: 'hidden' }, moves: [] },
};

// The board's weather moods (tw_mood_t order) and their pictures: partly cloudy has clear's (the sky differs)
const WEATHER = [['clear_day', 'sunny'], ['partly_day', 'sunny'], ['overcast', 'mug'], ['fog', 'fog'],
                 ['drizzle', 'drizzle'], ['rain', 'rain'], ['snow', 'snow'], ['thunder', 'thunder'],
                 ['clear_night', 'night'], ['partly_night', 'night'], ['plain', 'plain'], ['hidden', 'hidden']];

function weather() {
    // each picture drawn once: moods that look the same list the same files (the frames are stored once)
    const drawn = {};
    const files = (name, make) =>
        drawn[name] || (drawn[name] = make().map((f, i) => ({ file: save(name, f.svg, i), ms: f.ms })));
    const clips = [], moods = {};
    for (const [mood, name] of WEATHER) {
        const look = LOOKS[name];
        const add = (kind, what, make) =>
            clips.push({ name: `${mood}-${what}`, mood, kind, frames: files(`${name}-${what}`, make) });
        add('rest', 'rest', () => weatherRest(look));
        for (const move of look.moves)
            add('clip', move, () => MOVES[move](look));
        if (look.reaction)
            add('reaction', look.reaction, () => MOVES[look.reaction](look));
        moods[mood] = { pause: WEATHER_PAUSE,
                        weights: Object.fromEntries(look.moves.map(m => [`${mood}-${m}`, WEATHER_WEIGHT])) };
    }
    return { moods, clips };
}

// ---------------------------------------------------------------- the manifest

const drawnSet = set === 'weather' ? weather() : banner();
const manifest = { talpy_den: commit(), moods: drawnSet.moods, clips: drawnSet.clips };
writeFileSync(join(outdir, 'manifest.json'), JSON.stringify(manifest, null, 1) + '\n');
