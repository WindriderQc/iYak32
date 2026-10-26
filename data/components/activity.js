(() => {
    const U = BoardUI, I = IOInsights, $ = id => document.getElementById(id), query = new URLSearchParams(location.search);
    let events = [], session = query.get('session') || '', cursor = 0, lost = 0, online = false, focusTrace = query.get('trace'), focusAlert = query.get('alert'), lastSuccess = 0;
    $('activityGPIO').append(...IOModel.GPIOS.map(g => new Option(`GPIO ${g}`, g))); if (IOModel.GPIOS.includes(Number(query.get('gpio'))) && query.has('gpio')) $('activityGPIO').value = query.get('gpio');
    const matches = g => {
        const source = $('activitySource').value, type = $('activityType').value, gpio = $('activityGPIO').value, search = $('activitySearch').value.trim().toLowerCase();
        return (!focusTrace || String(g.trace) === focusTrace) && (!focusAlert || g.events.some(e => e.ref === focusAlert && e.category.startsWith('alert_'))) && (!source || g.events.some(e => e.source === source)) && (!gpio || g.events.some(e => e.gpio === Number(gpio))) && (!type || g.events.some(e => type === 'refused' ? e.ok === false : type === 'alert' ? e.category.startsWith('alert_') : e.category === type)) && (!search || g.events.some(e => `${I.describe(e)} ${I.SOURCES[e.source] || e.source} ${e.scene || ''}`.toLowerCase().includes(search)));
    };
    function render() {
        const container = $('activityGroups'), opened = new Set([...container.querySelectorAll('details[open]')].map(e => e.dataset.group)); container.replaceChildren();
        const groups = I.groups(events).filter(matches); $('activityEmpty').hidden = !!groups.length; $('activityEmpty').textContent = events.length ? 'Aucune activité pour ces filtres.' : 'Aucun événement dans le journal reçu.'; $('activityExport').disabled = !groups.length;
        groups.forEach(g => {
            const first = g.events[0], last = g.events.at(-1), main = g.events.find(e => ['rule', 'schedule', 'scene', 'alert_active', 'alert_clear'].includes(e.category)) || first;
            const card = document.createElement('details'); card.className = 'control-card activity-group'; card.dataset.group = g.key; card.dataset.trace = g.trace; card.open = opened.has(g.key) || !!focusTrace || !!focusAlert;
            const summary = document.createElement('summary'); summary.textContent = `${main.label}${main.category === 'gpio' ? ' · GPIO ' + main.gpio : ''} · ${I.SOURCES[main.source] || main.source} · ${new Date(last.at).toLocaleTimeString('fr-CA')}${g.events.some(e => e.ok === false) ? ' · Refus' : ''} · ${g.events.length} étape(s)`;
            const list = document.createElement('ol'); list.className = 'io-detail-events';
            g.events.forEach(e => {
                const li = document.createElement('li'); li.dataset.eventId = e.id; li.dataset.category = e.category;
                const text = document.createElement('strong'); text.textContent = I.describe(e); if (e.ok === false) text.dataset.state = 'error';
                const note = document.createElement('small'); note.textContent = `${new Date(e.at).toLocaleTimeString('fr-CA')} · ${I.SOURCES[e.source] || e.source}${e.scene ? ' · ' + e.scene : ''}`; li.append(text, note);
                const a = document.createElement('a'); a.href = e.category === 'gpio' ? `/io_detail.html?gpio=${e.gpio}` : e.category.startsWith('alert_') ? `/alerts.html?id=${encodeURIComponent(e.ref)}` : e.category === 'scene' ? `/io_control.html?scene=${encodeURIComponent(e.ref)}#sceneSection` : `/automation.html?id=${encodeURIComponent(e.ref)}`; a.textContent = 'Voir'; li.append(a); list.append(li);
            });
            card.append(summary, list); container.append(card);
        });
        $('activityStatus').textContent = `${online ? 'Journal en ligne' : 'Journal inaccessible · derniers événements reçus conservés'}${lost ? ` · ${lost} événement(s) remplacé(s)` : ''}${!online && lastSuccess ? ' · ' + U.age(Date.now() - lastSuccess) : ''}${focusTrace || focusAlert ? ' · séquence sélectionnée' : ''}`;
    }
    async function refresh() {
        try {
            const h = await U.json(`/api/io/activity?after=${cursor}&session=${encodeURIComponent(session)}`);
            if (!Array.isArray(h.events) || typeof h.session !== 'string') throw new Error('Journal invalide');
            if (h.reset || session && session !== h.session) { events = []; cursor = 0; lost = 0; if (session && session !== h.session) { focusTrace = null; U.notice('Redémarrage du board : nouveau journal.'); } }
            if (h.lost) events = [];
            h.events.filter(e => e.id > cursor).forEach(e => events.push({ ...e, at: Date.now() - ((h.now_ms - e.at_ms) >>> 0) })); events = events.slice(-64);
            if (session && session !== h.session) { const url = new URL(location.href); url.searchParams.delete('trace'); url.searchParams.delete('session'); history.replaceState(null, '', url); }
            session = h.session; cursor = h.latest_id; lost += h.lost || 0; online = true; lastSuccess = Date.now(); U.badge('boardBadge', 'Board en ligne', 'ok');
        } catch (_) { online = false; U.badge('boardBadge', 'Journal inaccessible', 'error'); }
        render();
    }
    ['activitySource', 'activityType', 'activityGPIO'].forEach(id => $(id).addEventListener('change', render)); $('activitySearch').addEventListener('input', render);
    $('clearFilters').addEventListener('click', () => { focusTrace = focusAlert = null; ['activitySource', 'activityType', 'activityGPIO', 'activitySearch'].forEach(id => $(id).value = ''); history.replaceState(null, '', location.pathname); render(); });
    $('activityExport').addEventListener('click', () => U.download('io-activity.json', JSON.stringify({ session, lost, events: I.groups(events).filter(matches).flatMap(g => g.events) }, null, 2), 'application/json'));
    const poll = U.poll(refresh, 2000); $('refreshButton').addEventListener('click', poll);
})();
