(() => {
    const U = BoardUI, M = IOModel, $ = id => document.getElementById(id), clone = x => JSON.parse(JSON.stringify(x));
    let config = null, runtime = null, draft = [], editing = null, conflict = false, online = false, busy = false;
    const dirty = () => M.signature(draft) !== M.signature(config?.alerts || []);
    const unit = a => a.source === 'temperature' ? '°C' : config?.io_pins.find(p => p.gpio === a.gpio)?.calibration?.unit || 'ADC';
    const sourceName = a => a.source === 'temperature' ? 'Température ambiante' : `${config?.io_pins.find(p => p.gpio === a.gpio)?.label || 'Sonde retirée'} · GPIO ${a.gpio}`;
    function render() {
        const changed = dirty(), error = config ? M.validate({ ...config, alerts: draft }) : '';
        $('saveBar').hidden = !changed; $('draftError').textContent = error; $('saveButton').disabled = busy || !online || !changed || conflict || !!editing || !!error;
        $('newAlert').disabled = busy || !online || draft.length >= 12; $('discardButton').disabled = busy; $('conflictPanel').hidden = !conflict;
        if (conflict) $('conflictDetail').textContent = `Board : ${JSON.stringify(config.alerts || [], null, 2)}\nBrouillon : ${JSON.stringify(draft, null, 2)}`;
        $('alertSummary').textContent = `${draft.length} alerte(s)${changed ? ' · brouillon' : ''}`; $('alertsEmpty').hidden = !!draft.length; $('alertCards').replaceChildren();
        draft.forEach(a => {
            const card = document.createElement('article'); card.className = 'control-card'; card.dataset.id = a.id; if (a.id === new URLSearchParams(location.search).get('id')) card.classList.add('linked-item');
            const h = document.createElement('h3'); h.textContent = a.name;
            const note = document.createElement('p'); note.className = 'panel-note'; note.textContent = `${sourceName(a)} ${a.operator === 'above' ? '>' : '<'} ${a.threshold} ${unit(a)} · marge ${a.hysteresis} · stabilité ${a.stable_ms / 1000} s`;
            const s = runtime?.items?.find(s => s.id === a.id), status = document.createElement('p'); status.className = 'control-status'; status.dataset.state = s?.active ? 'error' : 'ok';
            status.textContent = changed ? 'Brouillon · pas encore appliqué' : !online ? 'État inconnu' : ({ disabled: 'Désactivée', unavailable: 'Mesure indisponible', active: 'Seuil dépassé', pending: 'Stabilité en cours', normal: 'Dans la plage' })[s?.state] || 'En attente';
            if (!changed && online && s?.value != null) status.textContent += ` · ${U.number(s.value)} ${unit(a)}`;
            if (!changed && online && s?.stable_remaining_ms) status.textContent += ` · stabilité encore ${Math.ceil(s.stable_remaining_ms / 1000)} s`;
            const edit = document.createElement('button'); edit.className = 'btn secondary'; edit.textContent = 'Modifier'; edit.disabled = busy || !config; edit.addEventListener('click', () => open(a));
            const history = document.createElement('a'); history.className = 'btn secondary'; history.href = `/activity.html?alert=${encodeURIComponent(a.id)}`; history.textContent = 'Voir les changements';
            const actions = document.createElement('div'); actions.className = 'toolbar'; actions.append(edit, history); card.append(h, note, status, actions); $('alertCards').append(card);
        });
    }
    function hint() {
        const a = { source: $('alertSource').value === 'temperature' ? 'temperature' : 'gpio', gpio: Number($('alertSource').value) }, threshold = Number($('alertThreshold').value), margin = Number($('alertMargin').value);
        $('alertHint').textContent = `Unité : ${unit(a)}. Retour dans la plage ${$('alertOperator').value === 'above' ? '≤ ' + (threshold - margin) : '≥ ' + (threshold + margin)}, après stabilité. Les GPIO utilisent la conversion enregistrée ; sans calibration, le seuil est en ADC brut.`;
    }
    function open(a = null) {
        editing = { id: a?.id || M.newId('alert'), existing: !!a }; $('editor').hidden = false;
        $('alertSource').replaceChildren(new Option('Température ambiante · °C', 'temperature'), ...config.io_pins.filter(p => p.type === 'ANALOG_INPUT' && !M.output(p)).map(p => new Option(`${p.label} · GPIO ${p.gpio}`, p.gpio)));
        $('alertSource').value = a?.source === 'gpio' ? String(a.gpio) : 'temperature'; $('alertName').value = a?.name || 'Nouvelle alerte'; $('alertEnabled').checked = a?.enabled || false;
        $('alertOperator').value = a?.operator || 'above'; $('alertThreshold').value = a?.threshold ?? 30; $('alertMargin').value = a?.hysteresis ?? 2; $('alertStable').value = (a?.stable_ms ?? 2000) / 1000;
        $('deleteAlert').disabled = !a || busy; hint(); render(); $('editor').scrollIntoView({ behavior: 'smooth' }); $('alertName').focus();
    }
    function close() { editing = null; $('editor').hidden = true; render(); }
    async function refresh() {
        const result = await Promise.allSettled([U.json('/api/io/config'), U.json('/api/io/alerts/status')]);
        if (result.some(r => r.status !== 'fulfilled')) { online = false; U.badge('boardBadge', 'Board inaccessible', 'error'); render(); return; }
        const next = result[0].value;
        if (!config || !dirty()) draft = clone(next.alerts || []);
        else if (M.signature(config.alerts || []) !== M.signature(next.alerts || []) && M.signature(draft) !== M.signature(next.alerts || [])) conflict = true;
        config = next; runtime = result[1].value; online = !runtime._revision || runtime._revision === config._revision;
        U.badge('boardBadge', online ? 'Board en ligne' : 'Synchronisation en cours', online ? 'ok' : 'warning'); render();
    }
    $('alertForm').addEventListener('submit', e => {
        e.preventDefault(); if (!editing || busy) return;
        const a = { id: editing.id, name: $('alertName').value.trim(), enabled: $('alertEnabled').checked, source: $('alertSource').value === 'temperature' ? 'temperature' : 'gpio', operator: $('alertOperator').value, threshold: Number($('alertThreshold').value), hysteresis: Number($('alertMargin').value), stable_ms: Math.round(Number($('alertStable').value) * 1000) };
        if (a.source === 'gpio') a.gpio = Number($('alertSource').value);
        const next = draft.filter(item => item.id !== a.id).concat(a), error = M.validate({ ...config, alerts: next }); if (error) return U.notice(error, true);
        draft = next; close(); U.notice('Alerte dans le brouillon. Enregistrez pour l’appliquer.');
    });
    $('saveButton').addEventListener('click', async () => {
        if (!online || busy || conflict || editing) return; const next = { ...config, alerts: clone(draft) }, error = M.validate(next); if (error) return U.notice(error, true);
        busy = true; render();
        try { await U.json('/api/io/config', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(next) }); config = await U.json('/api/io/config'); draft = clone(config.alerts || []); conflict = false; U.notice('Alertes enregistrées. Aucune sortie commandée.'); }
        catch (e) { U.notice(`Enregistrement non confirmé : ${e.message}`, true); }
        finally { busy = false; await refresh(); }
    });
    $('newAlert').addEventListener('click', () => open()); $('closeEditor').addEventListener('click', close);
    $('deleteAlert').addEventListener('click', () => { if (!editing || busy) return; draft = draft.filter(a => a.id !== editing.id); close(); });
    $('discardButton').addEventListener('click', () => { draft = clone(config.alerts || []); conflict = false; close(); }); $('useBoard').addEventListener('click', () => $('discardButton').click()); $('keepDraft').addEventListener('click', () => { conflict = false; render(); });
    ['alertSource', 'alertOperator', 'alertThreshold', 'alertMargin'].forEach(id => $(id).addEventListener('input', hint));
    const poll = U.poll(refresh, 2000); $('refreshButton').addEventListener('click', poll); window.addEventListener('beforeunload', e => { if (dirty() || editing) { e.preventDefault(); e.returnValue = ''; } });
})();
