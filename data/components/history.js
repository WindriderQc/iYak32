(() => {
    const ui = BoardUI;
    let allData = [], range = 120;
    const measures = [ ['temp', 'Température', '°C', 'var(--text-accent)'], ['pres', 'Pression', 'hPa', '#63b3ed'],
        ['hum', 'Humidité', '%', '#81c784'], ['batt', 'Batterie', 'V', '#ffb74d'], ['rssi', 'Signal Wi-Fi', 'dBm', '#b39ddb'] ];
    function filtered() {
        if (!allData.length || !range) return allData;
        return allData.filter(row => row.t >= allData.at(-1).t - range * 1000);
    }
    function render() {
        const rows = filtered(), container = document.getElementById('historyCharts'); container.replaceChildren();
        measures.forEach(([key, label, unit, color]) => {
            if (key === 'hum' && !allData.some(row => typeof row.hum === 'number')) return;
            const panel = document.createElement('section'); panel.className = 'panel';
            const head = document.createElement('div'); head.className = 'panel-head';
            const title = document.createElement('h2'); title.textContent = label;
            const value = document.createElement('span'); value.className = 'history-value';
            value.textContent = ui.number(rows.at(-1)?.[key], key === 'rssi' ? 0 : key === 'batt' ? 2 : 1) + ' ';
            const suffix = document.createElement('small'); suffix.textContent = unit; value.append(suffix);
            head.append(title, value);
            const chart = document.createElement('div'); chart.className = 'chart'; ui.chart(chart, rows, key, unit, color);
            const stats = document.createElement('p'); stats.className = 'history-stats';
            const values = rows.map(row => row[key]).filter(v => typeof v === 'number' && Number.isFinite(v));
            stats.textContent = values.length ? `Min ${ui.number(Math.min(...values))} · Max ${ui.number(Math.max(...values))} ${unit}` : 'Aucune donnée pour cette période.';
            panel.append(head, chart, stats); container.append(panel);
        });
        ui.text('historySummary', allData.length > 1 ? `${allData.length} mesures disponibles · ${ui.number((allData.at(-1).t - allData[0].t) / 60000)} minutes sur le board` : 'L’historique se remplit à chaque mesure.');
        document.getElementById('exportButton').disabled = !rows.length;
    }
    async function refresh() {
        try {
            allData = await ui.json('/api/logs/sensors'); render();
            ui.badge('historyBadge', allData.length ? 'Mesures disponibles' : 'En attente de mesures', allData.length ? 'ok' : 'neutral');
            ui.text('lastUpdated', 'Actualisé à ' + new Date().toLocaleTimeString('fr-CA'));
        } catch (_) { ui.badge('historyBadge', 'Board inaccessible · données conservées', 'warning'); }
    }
    document.querySelectorAll('[data-range]').forEach(button => button.addEventListener('click', () => {
        range = Number(button.dataset.range);
        document.querySelectorAll('[data-range]').forEach(b => {
            const active = b === button; b.classList.toggle('active', active); b.classList.toggle('secondary', !active); b.setAttribute('aria-pressed', String(active));
        }); render();
    }));
    document.getElementById('exportButton').addEventListener('click', () => {
        const keys = ['t', 'temp', 'pres', 'hum', 'batt', 'heap', 'rssi'];
        const csv = ['uptime_ms,temperature_c,pressure_hpa,humidity_pct,battery_v,heap_bytes,wifi_rssi_dbm',
            ...filtered().map(row => keys.map(key => row[key] ?? '').join(','))].join('\r\n');
        ui.download('iyak32-mesures.csv', csv, 'text/csv;charset=utf-8');
    });
    render(); ui.poll(refresh, 10000);
})();
