// Throwaway mockup renderer: Talpy Den's Talpy with weather props, one SVG per scene.
// node render.mjs <talpy-den> <outdir>
import { mkdirSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';
import { pathToFileURL } from 'node:url';
import { SvgContext } from './svgctx.mjs';

const [den, out] = process.argv.slice(2);
mkdirSync(out, { recursive: true });
const ui = join(den, 'package/contents/ui/talpy');
const { Talpy } = await import(pathToFileURL(join(ui, 'talpy.mjs')).href);
const art = await import(pathToFileURL(join(ui, 'art.mjs')).href);

const INK = art.INK;
const OUT = '#6b5b3e';

function shades(ctx) {
    art.rrect(ctx, 19.8, 33.4, 10, 7.2, 2.8, INK);
    art.rrect(ctx, 34.2, 33.4, 10, 7.2, 2.8, INK);
    art.line(ctx, 'M29.8 35.8h4.4', INK, 1.3);
    art.line(ctx, 'M19.9 35.4l-4.4-1.2M44.1 35.4l4.4-1.2', INK, 1.1);
    art.line(ctx, 'M21.6 35.4h2.6M36 35.4h2.6', '#ffffff', 0.9, 0.55);
}

const CANOPY = 'M-24 34C-23 26-13 21 0 21C13 21 23 26 24 34Q18 31 12 34Q6 31 0 34Q-6 31-12 34Q-18 31-24 34Z';
function umbrella(ctx, A) {
    art.line(ctx, 'M0 6.5L0 34', OUT, 1.5);
    art.line(ctx, 'M0 6.5q0-3.2-2.6-3.2q-2.2 0-2.2 2.2', OUT, 1.5);
    ctx.save();
    art.about(ctx, 0, 34, -A);
    art.path(ctx, CANOPY, '#ff6b8b');
    art.path(ctx, 'M0 21Q-7 25-12 34Q-6 31 0 34Z', '#e2506f');
    art.path(ctx, 'M0 21Q14 23 24 34Q18 31 12 34Q8 26 0 21Z', '#e2506f');
    art.line(ctx, CANOPY, '#c23f5c', 0.8);
    art.line(ctx, 'M0 21Q-7 25-12 34M0 21Q7 25 12 34', '#c23f5c', 0.6);
    art.ellipse(ctx, -14, 27, 3.4, 1.5, '#ffffff', 0.4);
    art.circle(ctx, 0, 20.2, 1.1, OUT);
    ctx.restore();
}

const BLADE = 'M-21 33C-23 24-11 16 0 16C12 16 23 23 21 32C14 29 6 30 0 33C-6 30-14 30-21 33Z';
function leaf(ctx, A) {
    art.line(ctx, 'M0 6.5Q1.5 18 0 30', '#4f8f3a', 1.5);
    ctx.save();
    art.about(ctx, 0, 30, -A);
    art.path(ctx, BLADE, '#62b85c');
    art.path(ctx, 'M0 16C12 16 23 23 21 32C14 29 6 30 0 33Q2 24 0 16Z', '#52a24d');
    art.line(ctx, BLADE, '#3d8a3f', 0.8);
    art.line(ctx, 'M0 16Q2 24 0 33M0.6 21Q-7 21-13 25M0.8 25.5Q-8 25.5-17 29.5M0.8 21Q7.5 21 14 24.5M1 25.5Q9 25.5 17 28.5', '#3d8a3f', 0.6);
    art.ellipse(ctx, -10, 22, 3.2, 1.3, '#ffffff', 0.3);
    ctx.restore();
}

function rainHat(ctx) {
    const crown = 'M20.5 25C20.5 16 26 12.5 32 12.5S43.5 16 43.5 25Z';
    const brim = 'M13.5 26.5C18 23.4 46 23.4 50.5 26.5C52.6 28.2 50.6 30.6 47 30.2C40 28.6 24 28.6 17 30.2C13.4 30.6 11.4 28.2 13.5 26.5Z';
    art.path(ctx, crown, '#f2c94c'); art.line(ctx, crown, '#c49a1e', 0.8);
    art.path(ctx, brim, '#f2c94c'); art.line(ctx, brim, '#c49a1e', 0.8);
    art.line(ctx, 'M21 24.6C26 23.2 38 23.2 43 24.6', '#c49a1e', 0.8);
    art.ellipse(ctx, 26, 17.5, 2.2, 1.2, '#ffffff', 0.4);
}

function beanie(ctx) {
    const crown = 'M19.5 26C19.5 15 25 11 32 11S44.5 15 44.5 26Z';
    art.path(ctx, crown, '#ff6b8b'); art.line(ctx, crown, '#c23f5c', 0.8);
    art.line(ctx, 'M25 13.5Q24 19 24.5 25M32 11.3V25M39 13.5Q40 19 39.5 25', '#e2506f', 0.7);
    art.rrect(ctx, 18.4, 23.6, 27.2, 5.2, 2.6, '#e9edf2', '#c4cbdc', 0.7);
    art.circle(ctx, 24, 11.6, 3.3, '#e9edf2');
    art.circle(ctx, 24, 11.6, 3.3, 'rgba(0,0,0,0)');
}

function scarf(ctx) {
    const band = 'M13.6 43.6C24 47.2 40 47.2 50.4 43.6L50.6 48.8C40 52.4 24 52.4 13.4 48.8Z';
    art.path(ctx, band, '#ff6b8b'); art.line(ctx, band, '#c23f5c', 0.8);
    art.line(ctx, 'M20 45.6V50.6M27 46.6V51.6M34 46.8V51.8M41 46.2V51.2', '#ffffff', 0.9, 0.6);
    art.rrect(ctx, 37.5, 47.6, 5.6, 11, 1.8, '#ff6b8b', '#c23f5c', 0.8);
    art.line(ctx, 'M38.3 52.2h4M38.3 55.2h4', '#ffffff', 0.9, 0.6);
}

class WTalpy extends Talpy {
    constructor(o) { super(o); this.face = null; this.held = null; }
    drawFace(ctx, now, st, v, pal, move, ms) {
        super.drawFace(ctx, now, st, v, pal, move, ms);
        if (this.face) this.face(ctx);
    }
    drawArm(ctx, x, angle, dy, pal, holds, now) {
        if (this.held && x > 32) {
            const A = this.held.angle;
            ctx.save();
            ctx.translate(x, 41 + dy);
            ctx.rotate(A * Math.PI / 180);
            art.arm(ctx, pal);
            this.held.draw(ctx, A);
            ctx.restore();
            return;
        }
        super.drawArm(ctx, x, angle, dy, pal, holds, now);
    }
}

const START = 1e6, STEP = 100;
function draw(name, mood, step, face, held) {
    const t = new WTalpy({ brain: false, playful: false, theme: { fx: '#8C96A3', text: '#E9EDF2' } });
    t.nextBlink = t.nextGlance = START + 1e9;
    t.tick(START);
    t.setMood(mood);
    t.pose(Object.assign({ lamp: 'idle' }, step));
    for (let i = 1; i <= 60; i++) t.tick(START + i * STEP);
    t.face = face || null;
    t.held = held || null;
    const ctx = new SvgContext();
    t.draw(ctx, true);
    writeFileSync(join(out, name + '.svg'), ctx.svg('-40 -40 144 144'));
}

const open = { eyes: 'open', mouth: 'smile', depth: 'half', arms: 'rest' };
draw('sunny', 'idle', open, shades);
draw('partly', 'idle', open, shades);
draw('overcast', 'idle', { prop: 'mug', arms: 'hold', depth: 'high', eyes: 'happy', mouth: 'small' });
draw('fog', 'idle', { eyes: 'squint', mouth: 'flat', depth: 'half', arms: 'rest' });
draw('drizzle-leaf', 'idle', open, null, { angle: 160, draw: leaf });
draw('drizzle-hat', 'idle', open, rainHat);
draw('rain', 'idle', open, null, { angle: 160, draw: umbrella });
draw('snow', 'idle', { eyes: 'happy', mouth: 'smile', depth: 'half', arms: 'rest' }, ctx => { beanie(ctx); scarf(ctx); });
draw('thunder', 'idle', { depth: 'peek', eyes: 'wide', mouth: 'o', arms: 'rest' });
draw('night', 'done', { eyes: 'closed', mouth: 'small', depth: 'low', plume: 'droop', arms: 'rest' });
console.log('ok');
