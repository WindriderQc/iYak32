/* Helpers for local pages, with no external dependencies. */
window.BoardUI = (() => {
    const number = (v, digits = 1) => typeof v === 'number' && Number.isFinite(v)
        ? v.toLocaleString('fr-CA', { minimumFractionDigits: digits, maximumFractionDigits: digits }) : '—';
    const text = (id, v) => { const el = document.getElementById(id); if (el) el.textContent = v; };
    const age = ms => ms == null ? 'aucune mesure' : ms < 2000 ? 'à l’instant' : `il y a ${Math.floor(ms / 1000)} s`;
    const uptime = s => s >= 86400 ? `${Math.floor(s / 86400)} j ${Math.floor(s % 86400 / 3600)} h`
        : s >= 3600 ? `${Math.floor(s / 3600)} h ${Math.floor(s % 3600 / 60)} min`
        : s >= 60 ? `${Math.floor(s / 60)} min` : `${s} s`;
    async function json(path, options = {}) {
        const controller = new AbortController(), timer = setTimeout(() => controller.abort(), 6000);
        try {
            const r = await fetch(path, { cache: 'no-store', ...options, signal: controller.signal });
            const data = await r.json();
            if (!r.ok) throw new Error(data.message || data.error || `HTTP ${r.status}`);
            return data;
        } finally { clearTimeout(timer); }
    }
    function badge(id, label, state = 'neutral') {
        const el = document.getElementById(id);
        if (el) { el.textContent = label; el.dataset.state = state; }
    }
    function poll(refresh, interval = 5000) {
        let running = false;
        const run = async () => {
            if (running || document.hidden) return;
            running = true;
            try { await refresh(); } finally { running = false; }
        };
        run(); const timer = setInterval(run, interval);
        document.addEventListener('visibilitychange', run);
        window.addEventListener('pagehide', () => clearInterval(timer), { once: true });
        return run;
    }
    function notice(message, error = false) {
        const el = document.getElementById('actionNotice');
        if (el) { el.textContent = message; el.dataset.state = error ? 'error' : 'ok'; el.hidden = !message; }
    }
    function download(name, content, type) {
        const url = URL.createObjectURL(new Blob([content], { type }));
        const link = document.createElement('a'); link.href = url; link.download = name; link.click();
        setTimeout(() => URL.revokeObjectURL(url), 1000);
    }
    function chart(container, rows, key, unit, color = 'var(--text-accent)', levels = null) {
        const valid = row => typeof row[key] === 'number' && Number.isFinite(row[key]);
        const values = rows.filter(valid); container.replaceChildren();
        if (values.length < 2) {
            const p = document.createElement('p'); p.className = 'empty-state';
            p.textContent = values.length ? 'La courbe apparaîtra à la prochaine mesure.' : 'Aucune mesure disponible.';
            container.append(p); return;
        }
        const ns = 'http://www.w3.org/2000/svg', svg = document.createElementNS(ns, 'svg');
        svg.setAttribute('viewBox', '0 0 640 180'); svg.setAttribute('role', 'img');
        svg.setAttribute('aria-label', `Évolution : ${number(values[0][key])} à ${number(values.at(-1)[key])} ${unit}`);
        const create = (tag, attrs) => {
            const el = document.createElementNS(ns, tag);
            Object.entries(attrs).forEach(([k, v]) => el.setAttribute(k, v)); svg.append(el); return el;
        };
        let low = Math.min(...values.map(r => r[key])), high = Math.max(...values.map(r => r[key]));
        const pad = Math.max((high - low) * .15, key === 'pres' ? .1 : .05); low -= pad; high += pad;
        if (levels) { low = -.1; high = 1.1; }
        const start = rows[0].t, span = Math.max(rows.at(-1).t - start, 1);
        const x = t => 64 + (t - start) / span * 560, y = v => 140 - (v - low) / (high - low) * 120;
        (levels ? [0, 1] : [0, .5, 1]).forEach(f => {
            if (levels) {
                create('line', { x1: 64, x2: 624, y1: y(1 - f), y2: y(1 - f), class: 'chart-grid' });
                create('text', { x: 54, y: y(1 - f) + 4, 'text-anchor': 'end', class: 'chart-label' }).textContent = levels[1 - f]; return;
            }
            create('line', { x1: 64, x2: 624, y1: 20 + f * 120, y2: 20 + f * 120, class: 'chart-grid' });
            create('text', { x: 54, y: 24 + f * 120, 'text-anchor': 'end', class: 'chart-label' }).textContent = number(high - f * (high - low), key === 'rssi' ? 0 : 1);
        });
        let segment = [];
        const draw = () => {
            if (segment.length) create('polyline', { points: segment.join(' '), fill: 'none', stroke: color,
                'stroke-width': 2.5, 'stroke-linecap': 'round', 'stroke-linejoin': 'round' });
            segment = [];
        };
        rows.forEach(r => valid(r) ? segment.push(`${x(r.t)},${y(r[key])}`) : draw()); draw();
        const last = values.at(-1); create('circle', { cx: x(last.t), cy: y(last[key]), r: 4, fill: color });
        create('text', { x: 64, y: 169, class: 'chart-label' }).textContent = `−${number(span / 60000)} min`;
        create('text', { x: 624, y: 169, 'text-anchor': 'end', class: 'chart-label' }).textContent = 'Dernière mesure';
        container.append(svg);
    }
    return { number, text, age, uptime, json, badge, poll, notice, download, chart };
})();
