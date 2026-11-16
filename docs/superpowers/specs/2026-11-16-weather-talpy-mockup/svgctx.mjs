
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
export { SvgContext };
