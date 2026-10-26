(() => {
    const ui = BoardUI;
    let traffic = { topics: [] }, paused = false, lastSuccess = 0;
    const list = document.getElementById('topicList'), filter = document.getElementById('topicFilter');
    const states = { connected: 'Connecté', connecting: 'Connexion…', backoff: 'Nouvelle tentative bientôt', disconnected: 'Déconnecté', disabled: 'Désactivé' };
    function render() {
        const open = new Set([...list.querySelectorAll('details[open]')].map(el => el.dataset.key));
        const query = filter.value.trim().toLowerCase(); list.replaceChildren();
        const rows = (traffic.topics || []).filter(row => `${row.topic} ${row.payload}`.toLowerCase().includes(query));
        ui.text('topicCount', `${rows.length} topics`);
        if (!rows.length) {
            const p = document.createElement('p'); p.className = 'empty-state';
            p.textContent = query ? 'Aucun topic ne correspond à ce filtre.' : 'Les premiers messages apparaîtront après la connexion MQTT.';
            list.append(p); return;
        }
        rows.sort((a, b) => a.topic.localeCompare(b.topic) || a.direction.localeCompare(b.direction)).forEach(row => {
            const details = document.createElement('details'); details.className = 'topic-card';
            details.dataset.key = row.direction + ':' + row.topic; details.open = open.has(details.dataset.key);
            const summary = document.createElement('summary'), direction = document.createElement('span');
            direction.className = 'topic-direction'; direction.textContent = row.direction === 'in' ? 'RX ↓' : 'TX ↑';
            const name = document.createElement('div'); name.className = 'topic-name'; name.textContent = row.topic;
            const preview = document.createElement('span'); preview.className = 'topic-preview';
            preview.textContent = row.payload.length < 100 ? row.payload : row.payload.slice(0, 90) + '…'; name.append(preview);
            const time = document.createElement('span'); time.className = 'topic-time';
            time.textContent = `${row.ok ? ui.age(row.age_ms) : 'Échec d’envoi'}${row.retained ? ' · conservé' : ''}`;
            if (!row.ok) time.dataset.state = 'error';
            const pre = document.createElement('pre');
            try { pre.textContent = JSON.stringify(JSON.parse(row.payload), null, 2); } catch (_) { pre.textContent = row.payload; }
            if (row.truncated) pre.textContent += '\n… aperçu tronqué';
            summary.append(direction, name, time); details.append(summary, pre); list.append(details);
        });
    }
    async function refresh() {
        if (paused) return;
        const [status, messages] = await Promise.allSettled([ui.json('/api/mqtt/status'), ui.json('/api/mqtt/traffic')]);
        if (status.status === 'fulfilled') {
            const d = status.value; lastSuccess = Date.now();
            ui.badge('mqttBadge', states[d.state] || 'État inconnu', d.connected ? 'ok' : d.enabled ? 'warning' : 'neutral');
            ui.text('brokerAddress', d.server ? `${d.server}:${d.port}` : 'Non configuré');
            ui.text('brokerDetail', d.connected ? 'Le ESP32 est connecté à ce broker.' : d.enabled ? `Tentatives : ${d.attempts} · code ${d.rc}` : 'MQTT est désactivé dans les réglages.');
            document.getElementById('reconnectButton').disabled = !d.enabled;
            ui.text('lastUpdated', 'Actualisé à ' + new Date().toLocaleTimeString('fr-CA'));
        } else {
            ui.badge('mqttBadge', 'Board inaccessible', 'error');
            document.getElementById('reconnectButton').disabled = true;
            ui.text('lastUpdated', lastSuccess ? `Dernière réponse ${ui.age(Date.now() - lastSuccess)}` : 'Aucune réponse du board');
        }
        if (messages.status === 'fulfilled') {
            traffic = messages.value;
            ui.text('sentCount', traffic.sent ?? '—'); ui.text('receivedCount', traffic.received ?? '—'); ui.text('failedCount', traffic.failed ?? '—');
            render();
        } else ui.notice('Les messages ne peuvent pas être actualisés. Les dernières valeurs affichées sont conservées.', true);
    }
    const run = ui.poll(refresh, 3000);
    filter.addEventListener('input', render);
    document.getElementById('pauseButton').addEventListener('click', event => {
        paused = !paused; event.currentTarget.textContent = paused ? 'Reprendre' : 'Pause';
        event.currentTarget.setAttribute('aria-pressed', String(paused));
        if (paused) ui.text('lastUpdated', 'Vue en pause'); else run();
    });
    document.getElementById('copyButton').addEventListener('click', async () => {
        try { await navigator.clipboard.writeText(JSON.stringify(traffic, null, 2)); ui.notice('Messages copiés.'); }
        catch (_) { ui.download('iyak32-mqtt.json', JSON.stringify(traffic, null, 2), 'application/json'); ui.notice('JSON téléchargé.'); }
    });
    document.getElementById('reconnectButton').addEventListener('click', async event => {
        const button = event.currentTarget;
        button.disabled = true;
        try { await ui.json('/api/mqtt/reconnect', { method: 'POST' }); ui.notice('Reconnexion MQTT demandée.'); await run(); }
        catch (e) { ui.notice(`Reconnexion non confirmée : ${e.message}`, true); }
        finally { button.disabled = false; }
    });
})();
