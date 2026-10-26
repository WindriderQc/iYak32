(function (root) {
    const Model = typeof module === 'object' && module.exports ? require('./io-model.js') : root.IOModel;
    const TITLES = { temperature: 'Température', pressure: 'Pression', humidity: 'Humidité', battery_voltage: 'Batterie', wifi_rssi: 'Signal Wi-Fi', cpu_temperature: 'Température CPU', automation_pause: 'Pause des automatismes', scene_all_off: 'Tout OFF' };
    function available(config) { return !!config?.home_assistant && !Model.homeAssistantError(config.home_assistant); }
    function entries(config) {
        if (!available(config)) return [];
        return Object.entries(config.home_assistant.entities).map(([key, id]) => ({ key, id, label: TITLES[key] || (key.startsWith('gpio') ? `${config.io_pins?.find(p => p.gpio === Number(key.slice(4)))?.label || key} · GPIO ${key.slice(4)}` : key.startsWith('scene_') ? config.scenes?.find(s => s.id === key.slice(6))?.name || key : key) }));
    }
    function history(config, key = '', hours = 24, now = Date.now()) {
        if (!available(config) || ![24, 168].includes(hours) || !Number.isFinite(now)) return '';
        const h = config.home_assistant, ids = key ? h.entities[key] ? [h.entities[key]] : [] : [...new Set(Object.values(h.entities).filter(id => !id.startsWith('button.')))];
        if (!ids.length) return '';
        const url = new URL('/history', h.url); url.searchParams.set('entity_id', ids.join(',')); url.searchParams.set('start_date', new Date(now - hours * 3600000).toISOString()); url.searchParams.set('end_date', new Date(now).toISOString()); return url.href;
    }
    function entity(config, key) {
        if (!available(config) || !config.home_assistant.entities[key]) return '';
        const url = new URL('/lovelace', config.home_assistant.url); url.searchParams.set('more-info-entity-id', config.home_assistant.entities[key]); return url.href;
    }
    function device(config) { return available(config) && config.home_assistant.device_id ? new URL('/config/devices/device/' + config.home_assistant.device_id, config.home_assistant.url).href : ''; }
    function link(element, href) {
        if (!element) return; element.hidden = !href; element.style.display = href ? '' : 'none'; if (href) { element.href = href; element.target = '_blank'; element.rel = 'noopener noreferrer'; }
        else element.removeAttribute('href');
    }
    function historyPanel() {
        const U = BoardUI, M = IOModel, $ = id => document.getElementById(id); if (!$('haHistoryPanel')) return;
        let config = null, dirty = false, editRevision = '', saving = false, current = null, online = false;
        function draft() {
            const entities = {};
            $('haEntityFields').querySelectorAll('input[data-key]').forEach(input => { if (input.value.trim()) entities[input.dataset.key] = input.value.trim(); });
            return { url: $('haURL').value.trim(), device_id: $('haDevice').value.trim(), entities };
        }
        function fill() {
            const h = config?.home_assistant; $('haURL').value = h?.url || ''; $('haDevice').value = h?.device_id || ''; $('haEntityFields').replaceChildren();
            const keys = [...new Set([...Object.keys(TITLES), ...(config?.io_pins || []).map(p => 'gpio' + p.gpio), ...Object.keys(h?.entities || {})])];
            keys.forEach(key => { const label = document.createElement('label'), input = document.createElement('input'); input.dataset.key = key; input.value = h?.entities?.[key] || ''; input.placeholder = 'Entité HA, ou vide'; input.maxLength = 192;
                const title = TITLES[key] || entries(config).find(e => e.key === key)?.label || (key.startsWith('gpio') ? `${config?.io_pins.find(p => p.gpio === Number(key.slice(4)))?.label || key} · GPIO ${key.slice(4)}` : key); label.append(title, input); $('haEntityFields').append(label); });
            editRevision = config?._revision || '';
        }
        function render() {
            const before = $('haEntityChoice').value, items = entries(config);
            $('haEntityChoice').replaceChildren(new Option('Toutes les mesures et I/O', ''), ...items.map(e => new Option(e.label, e.key))); if (items.some(e => e.key === before)) $('haEntityChoice').value = before;
            $('haEntityChoice').disabled = !items.length; link($('haOpenDevice'), device(config)); updateLinks();
            $('haStatus').textContent = available(config) ? `${items.length} entité(s) liée(s) · ${new URL(config.home_assistant.url).host}${!online ? ' · configuration conservée, board inaccessible' : ''}` : online ? 'Configurez l’adresse et les entités HA pour ouvrir leur historique.' : 'Liens HA indisponibles · attente du board.';
            $('haSave').disabled = !config || !online || saving || !dirty; $('haReload').disabled = !config || saving;
        }
        function updateLinks() { const key = $('haEntityChoice').value; link($('ha24h'), history(config, key, 24)); link($('ha7d'), history(config, key, 168)); link($('haOpenEntity'), key ? entity(config, key) : ''); }
        async function refresh() {
            try { const next = await U.json('/api/io/config'); current = next; online = true; if (!dirty) { config = next; fill(); } else { config = next; } render(); }
            catch (_) { online = false; render(); }
        }
        $('haSettingsForm').addEventListener('input', () => { if (!dirty) editRevision = current?._revision || ''; dirty = true; render(); });
        $('haEntityChoice').addEventListener('change', updateLinks);
        for (const id of ['ha24h', 'ha7d', 'haOpenEntity']) $(id).addEventListener('click', updateLinks);
        $('haReload').addEventListener('click', () => { dirty = false; config = current; fill(); render(); U.notice('Liens du board rechargés.'); });
        $('haSettingsForm').addEventListener('submit', async e => {
            e.preventDefault(); if (!config || !online || saving) return;
            const h = draft(), error = M.homeAssistantError(h); if (error) return U.notice(error, true); h.url = new URL(h.url).origin;
            const next = { ...config, _revision: editRevision, home_assistant: h }, invalid = M.validate(next); if (invalid) return U.notice(invalid, true);
            saving = true; render();
            try { await U.json('/api/io/config', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(next) }); dirty = false; U.notice('Liens Home Assistant enregistrés.'); await refresh(); }
            catch (err) { U.notice(`Liens non enregistrés : ${err.message}`, true); }
            finally { saving = false; render(); }
        });
        U.poll(refresh, 10000); document.addEventListener('visibilitychange', updateLinks); render();
        window.addEventListener('beforeunload', e => { if (dirty) { e.preventDefault(); e.returnValue = ''; } });
    }
    const api = { available, entries, history, entity, device, link, historyPanel };
    if (typeof module === 'object' && module.exports) module.exports = api; else { root.HALinks = api; if (root.document) historyPanel(); }
})(typeof globalThis === 'object' ? globalThis : this);
