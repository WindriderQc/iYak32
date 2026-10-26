(function (root) {
    // Confirmation points only: no value before the first observation and no line across a gap.
    function timeline(events, samples, gpio, output, since = 0) {
        const rows = output ? events.filter(e => e.gpio === gpio && e.at >= since && !['override', 'resumed'].includes(e.kind)).map(e => ({
            t: e.at, value: e.kind === 'removed' || e.output === false ? null : typeof e.active === 'boolean' ? Number(e.active) : null, origin: e.source || 'journal'
        })) : [];
        rows.push(...samples);
        return rows.sort((a, b) => a.t - b.t);
    }
    function steps(rows) {
        const result = []; let previous = null;
        rows.forEach(row => {
            if (previous?.value != null && row.value != null) result.push({ ...previous, t: row.t });
            result.push(row); previous = row;
        }); return result;
    }
    function alerts(board, config, io, automation, thresholds = null) {
        const items = [], add = (key, text, href) => items.push({ key, text, href });
        if (!board) { add('board', 'Board inaccessible : valeurs et commandes non confirmées. Réessayez la connexion.', '/status.html'); return items; }
        if (!board.wifi_connected) add('wifi', 'Wi-Fi déconnecté : vérifier le réseau du board.', '/wifi.html');
        if (board.mqtt_enabled && board.mqtt_state !== 'connected') add('mqtt', 'MQTT déconnecté : vérifier le broker et les paramètres.', '/mqtt.html');
        if (board.sensor_age_ms == null || board.sensor_age_ms > Math.max(15000, (board.sample_interval_sec || 5) * 3000)) add('sensor', 'Mesure ambiante indisponible ou périmée : vérifier le capteur.', '/status.html');
        if (!config || !io || io._revision && io._revision !== config._revision) add('io', 'État des I/O non confirmé : actualisation nécessaire.', '/io_control.html');
        else (config.io_pins || []).forEach(pin => {
            const r = io.statuses?.find(r => r.gpio === pin.gpio);
            // GPIO21 is the board's reserved I²C SDA, also present in the original I/O configuration.
            if (pin.gpio === 21 && r?.value_error === 'PIN_IN_USE') return;
            if (typeof r?.value !== 'number') add(`pin:${pin.gpio}`, `${pin.label} · GPIO ${pin.gpio} : ${r?.reason || 'lecture indisponible'}.`, `/io_detail.html?gpio=${pin.gpio}`);
        });
        if (!automation) add('auto', 'État des automatismes indisponible : vérifier avant une reprise.', '/automation.html');
        else {
            const configured = config?.automation;
            if (!automation.paused && !automation.clock_valid && (configured?.schedules?.some(i => i.enabled) || configured?.rules?.some(i => i.enabled && i.window))) add('clock', 'Heure indisponible : horaires et règles avec plage suspendus.', '/automation.html');
            const seen = new Set(); let count = 0;
            // Only the latest attempt per automation, for ten minutes. Repeated polls do not add alerts.
            [...(automation.runs || [])].sort((a, b) => ((automation.now_ms - a.at_ms) >>> 0) - ((automation.now_ms - b.at_ms) >>> 0)).forEach(run => {
                const key = run.id || `${run.source}:${run.name}:${run.scene_id}`; if (seen.has(key)) return; seen.add(key);
                if (run.ok !== false || ((automation.now_ms - run.at_ms) >>> 0) > 600000 || count >= 5) return;
                count++; add(key, `${run.name} : ${run.message}`, '/automation.html');
            });
        }
        if (config?.alerts?.length) {
            if (!thresholds || thresholds._revision && thresholds._revision !== config._revision) add('thresholds', 'État des alertes de seuil non confirmé.', '/alerts.html');
            else config.alerts.filter(a => a.enabled).forEach(a => {
                const state = thresholds.items?.find(s => s.id === a.id);
                if (!state || state.state === 'unavailable') add(`threshold:${a.id}`, `${a.name} : mesure indisponible, seuil non évalué.`, `/alerts.html?id=${encodeURIComponent(a.id)}`);
                else if (state.active) add(`threshold:${a.id}`, `${a.name} : ${state.value} ${a.source === 'temperature' ? '°C' : config.io_pins.find(p => p.gpio === a.gpio)?.calibration?.unit || 'ADC'} · seuil ${a.operator === 'below' ? '<' : '>'} ${a.threshold}.`, `/alerts.html?id=${encodeURIComponent(a.id)}`);
            });
        }
        return items;
    }
    const SOURCES = { web: 'Web', mqtt: 'MQTT', button: 'Bouton', timer: 'Minuterie', config: 'Configuration', boot: 'Démarrage', input: 'Entrée', rule: 'Règle', schedule: 'Horaire' };
    function describe(e) {
        if (e.category !== 'gpio') return `${e.category === 'rule' ? 'Règle' : e.category === 'schedule' ? 'Horaire' : e.category === 'scene' ? 'Scène' : e.category === 'alert_active' ? 'Alerte active' : 'Alerte terminée'} · ${e.label} · ${e.message || ''}${typeof e.reading === 'number' ? ` · lecture ${e.reading}` : ''}`;
        const kind = { command: 'Commande', pulse: 'Activation temporisée', expired: 'Arrêt automatique', startup: 'Démarrage', configured: 'Configuration', removed: 'Retrait', input_changed: 'Entrée modifiée', override: 'Priorité manuelle', resumed: 'Priorité libérée' }[e.kind] || e.kind;
        return `${kind} · ${e.label} · GPIO ${e.gpio}${['command', 'pulse', 'expired'].includes(e.kind) ? ` · ${e.active ? 'ON' : 'OFF'}` : ''}${e.duration_ms ? ` · ${e.duration_ms / 1000} s` : ''}`;
    }
    function groups(events) {
        const map = new Map(); [...events].sort((a, b) => a.id - b.id).forEach(e => {
            const key = e.trace ? `trace:${e.trace}` : `event:${e.id}`;
            if (!map.has(key)) map.set(key, { key, trace: e.trace || 0, events: [] }); map.get(key).events.push(e);
        }); return [...map.values()].sort((a, b) => b.events.at(-1).id - a.events.at(-1).id);
    }
    function lastActivation(events, config, zone) {
        const gpios = new Set((config?.io_pins || []).filter(p => p.zone === zone).map(p => p.gpio));
        return events.filter(e => e.category === 'gpio' && e.output && e.active && ['command', 'pulse'].includes(e.kind) && gpios.has(e.gpio)).sort((a, b) => b.id - a.id)[0] || null;
    }
    const api = { timeline, steps, alerts, SOURCES, describe, groups, lastActivation };
    if (typeof module === 'object' && module.exports) module.exports = api; else root.IOInsights = api;
})(typeof globalThis === 'object' ? globalThis : this);
