(() => {
    const U = BoardUI, M = IOModel, $ = id => document.getElementById(id);
    const query = new URLSearchParams(location.search), requested = query.get('gpio');
    let gpio = requested !== null && /^(0|[1-9]\d*)$/.test(requested) && M.GPIOS.includes(Number(requested)) ? Number(requested) : null;
    let config = null, io = null, automation = null, online = false, busy = false, epoch = 0, lastSuccess = 0;
    let events = [], session = '', cursor = 0, lost = 0, journal = 'Connexion au journal…';
    let samples = [], seriesKey = '', since = 0, journalOnline = false, historyNote = '';
    const pin = () => config?.io_pins?.find(p => p.gpio === gpio);
    const reading = () => io?.statuses?.find(p => p.gpio === gpio);
    const known = () => online && typeof reading()?.value === 'number';
    const active = p => typeof p?.active === 'boolean' ? p.active : !!p?.value !== !!p?.inverted;
    const seconds = ms => `${Math.ceil(ms / 1000)} s`;
    const sources = { web: 'Web', mqtt: 'MQTT', button: 'Bouton', timer: 'Minuterie', config: 'Configuration', boot: 'Démarrage', input: 'Entrée', rule: 'Règle', schedule: 'Horaire' };
    $('detailGPIO').replaceChildren(new Option('Choisir une broche', ''), ...M.GPIOS.map(g => new Option(`GPIO ${g}`, g)));
    $('detailGPIO').value = gpio ?? '';
    if (requested !== null && gpio === null) U.notice('GPIO invalide. Choisissez une broche disponible.', true);
    function fact(label, value) { const dt = document.createElement('dt'), dd = document.createElement('dd'); dt.textContent = label; dd.textContent = value; $('pinFacts').append(dt, dd); }
    function association(list, text, href, description) {
        const li = document.createElement('li'), a = document.createElement('a'); a.textContent = text; a.href = href; li.append(a);
        if (description) { const p = document.createElement('p'); p.textContent = description; li.append(p); } list.append(li);
    }
    function observe() {
        const p = pin(), r = reading(), key = M.signature({ gpio, type: p?.type, mode: p?.mode, inverted: p?.inverted, calibration: p?.calibration });
        if (seriesKey && key !== seriesKey) { samples = []; since = Date.now(); historyNote = 'Broche ou conversion modifiée : nouvelle série.'; }
        seriesKey = key;
        const t = Date.now(), value = known() && (!M.output(p) || journalOnline) ? M.output(p) ? Number(active(r)) : r.calibrated_value ?? r.value : null;
        if (samples.length && t - samples.at(-1).t > 6000) samples.push({ t: samples.at(-1).t + 1, value: null, origin: 'interruption' });
        samples.push({ t, value, origin: value == null ? 'indisponible' : 'lecture' }); samples = samples.slice(-120);
    }
    function historyRows() { return IOInsights.timeline(events, samples, gpio, M.output(pin()), since); }
    function renderHistory() {
        const p = pin(), rows = historyRows(), valid = rows.filter(row => typeof row.value === 'number'), digital = p?.type !== 'ANALOG_INPUT';
        U.chart($('pinHistory'), digital ? IOInsights.steps(rows) : rows, 'value', digital ? M.output(p) ? 'ON/OFF' : 'HIGH/LOW' : p?.calibration?.unit || 'ADC', undefined, digital ? M.output(p) ? ['OFF', 'ON'] : ['LOW', 'HIGH'] : null);
        $('historyExport').disabled = !valid.length;
        $('historyStats').textContent = !valid.length ? 'En attente de valeurs confirmées.' : digital ? `${valid.length} confirmations · 1 = ${M.output(p) ? 'ON' : 'HIGH'}, 0 = ${M.output(p) ? 'OFF' : 'LOW'}` : `Min ${U.number(Math.min(...valid.map(r => r.value)))} · max ${U.number(Math.max(...valid.map(r => r.value)))} · ${valid.length} lectures`;
        $('pinHistoryNote').textContent = `${M.output(p) ? 'Sorties : transitions confirmées du journal RAM, y compris les impulsions courtes, et lectures de cet onglet.' : 'Entrées : jusqu’à 120 lectures dans cet onglet, environ toutes les 2 s. Une variation entre deux lectures peut être manquée.'} ${historyNote} Les interruptions coupent la courbe. Le redémarrage efface la série ; fermer l’onglet efface les lectures locales. Aucun historique de 24 h.`;
    }
    function render() {
        const p = pin(), r = reading(), output = M.output(p), available = known(), held = available && r.override_remaining_ms > 0;
        $('detailGPIO').disabled = busy;
        $('pinTitle').textContent = gpio === null ? 'Choisir une broche' : `${M.icon(p)} ${p?.label || `GPIO ${gpio}`}`;
        $('pinSubtitle').textContent = gpio === null ? 'Valeur, configuration et actions au même endroit.' : `GPIO ${gpio}${p?.zone ? ' · ' + p.zone : ''}${!p ? ' · non configurée' : ''}`;
        document.title = `iYak32 · ${p?.label || (gpio === null ? 'Fiche I/O' : `GPIO ${gpio}`)}`;
        for (const [id, route] of [['configurePin', '/io_control.html'], ['commissionPin', '/commission.html']]) { $(id).href = route + (gpio === null ? '' : `?gpio=${gpio}`); }
        U.badge('pinModeBadge', !online ? 'État inconnu' : !p ? 'Non configurée' : !available ? 'Lecture indisponible' : held ? 'Priorité manuelle' : output ? automation ? automation.paused ? 'Automatismes en pause' : 'Auto autorisé' : 'Automatismes inconnus' : 'Mesure en direct', available && !held ? 'ok' : 'warning');
        $('pinReading').textContent = !available ? '—' : output ? active(r) ? 'ON' : 'OFF' : p.type === 'ANALOG_INPUT' ? `${U.number(r.calibrated_value ?? r.value, r.calibration ? 1 : 0)} ${r.unit || (r.calibration ? r.calibration.unit : 'ADC') || ''}`.trim() : r.value ? 'HIGH' : 'LOW';
        $('pinReadingNote').textContent = !p ? 'Cette broche n’est pas lue tant qu’elle n’est pas configurée.' : !online ? 'Valeur actuelle non confirmée. Commandes désactivées.' : !available ? r?.reason || 'Lecture indisponible.' : p.type === 'ANALOG_INPUT' ? `ADC brut : ${r.value} · 0–4095` : `Niveau électrique : ${r.value ? 'HIGH' : 'LOW'}${r.remaining_ms ? ` · arrêt automatique dans ${seconds(r.remaining_ms)}` : ''}`;
        $('pinCommands').hidden = !output;
        for (const id of ['pinOn', 'pinOff', 'pinTest', 'pinPriority']) $(id).disabled = !available || !output || busy;
        $('pinOn').setAttribute('aria-pressed', String(available && active(r))); $('pinOff').setAttribute('aria-pressed', String(available && !active(r)));
        $('pinPriority').textContent = held ? `Reprendre · ${seconds(r.override_remaining_ms)}` : 'Prendre la main'; $('priorityDuration').disabled = !available || busy || held;
        $('priorityNote').textContent = held ? `Les scènes automatiques touchant cette sortie sont ignorées. Reprise dans ${seconds(r.override_remaining_ms)}, sans rattrapage.${automation?.paused ? ' La pause globale reste active.' : ''}` : automation?.paused ? 'Les automatismes sont en pause globale. Prendre ou libérer une priorité ne change pas cette pause.' : 'La priorité manuelle ignore les actions automatiques, sans empêcher l’arrêt d’une minuterie.';
        $('pinFacts').replaceChildren();
        if (gpio !== null) {
            fact('Rôle', p ? output ? 'Sortie numérique' : p.type === 'ANALOG_INPUT' ? 'Entrée analogique' : 'Entrée numérique' : 'Non configurée');
            if (p) { fact('Mode enregistré', p.mode); if (r?.mode && r.mode !== p.mode) fact('Mode effectif', r.mode); fact('Zone', p.zone || 'Aucune'); }
            const owners = io?.pins?.find(item => item.gpio === gpio)?.usages || []; if (owners.length) fact('Usages du firmware', owners.map(owner => owner.label).join(' · '));
            if (M.INPUT_ONLY.includes(gpio)) fact('Capacité', 'Entrée uniquement, sans résistance interne');
            if (output) { fact('Polarité', p.inverted ? 'ON = LOW · sortie inversée' : 'ON = HIGH'); fact('Au démarrage', p.initial_state || 'LOW'); fact('Minuterie', !available ? 'État inconnu' : r.remaining_ms ? `Arrêt dans ${seconds(r.remaining_ms)}` : 'Aucune'); fact('Priorité manuelle', !available ? 'État inconnu' : held ? `Encore ${seconds(r.override_remaining_ms)}` : 'Aucune'); }
        }
        const calibration = p?.calibration; $('calibrationSection').hidden = !calibration;
        $('calibrationDescription').textContent = calibration ? `ADC ${calibration.raw_min} → ${calibration.value_min} ${calibration.unit} ; ADC ${calibration.raw_max} → ${calibration.value_max} ${calibration.unit}. ${calibration.clamp !== false ? 'Valeur limitée à la plage.' : 'Extrapolation hors de la plage.'}` : '';
        const related = M.relationships(config, gpio); $('relatedScenes').replaceChildren(); $('relatedAutomation').replaceChildren();
        related.scenes.forEach(scene => { const action = scene.actions.find(a => a.gpio === gpio); association($('relatedScenes'), `${M.icon(scene)} ${scene.name}`, `/io_control.html?scene=${encodeURIComponent(scene.id)}#sceneSection`, `${action.active ? 'ON' : 'OFF'}${action.duration_ms ? ` pendant ${seconds(action.duration_ms)}` : ''}`); });
        for (const [kind, items] of [['Règle', related.rules], ['Horaire', related.schedules]]) items.forEach(item => {
            const state = automation?.items?.find(s => s.id === item.id), role = kind === 'Règle' && item.gpio === gpio ? 'Cette broche fournit la condition.' : 'La scène commande cette broche.';
            const detail = !item.enabled ? 'Désactivé.' : !state ? 'État actuel indisponible.' : kind === 'Règle' ? M.explainRule(item, state) : automation.paused ? 'Automatismes en pause.' : state.override_remaining_ms ? `Priorité manuelle encore ${seconds(state.override_remaining_ms)}.` : state.next_epoch ? 'Prochaine occurrence : ' + new Date(state.next_epoch * 1000).toLocaleString('fr-CA', { timeZone: automation.timezone }) : 'Aucune occurrence disponible.';
            association($('relatedAutomation'), `${kind} · ${item.name}`, `/automation.html?id=${encodeURIComponent(item.id)}`, `${role} ${kind === 'Horaire' ? item.time + ' · ' : ''}${detail}`);
        });
        $('scenesEmpty').hidden = !!related.scenes.length; $('automationEmpty').hidden = !!(related.rules.length + related.schedules.length);
        $('automationNote').textContent = !automation ? 'État des automatismes indisponible.' : automation.paused ? 'Pause globale active. Les commandes manuelles restent disponibles.' : 'Les actions automatiques restent évaluées sur le board, même sans navigateur.';
        $('pinEvents').replaceChildren(); const visible = events.filter(e => e.gpio === gpio).slice(-8).reverse();
        visible.forEach(event => {
            const li = document.createElement('li'), label = document.createElement('strong'), detail = document.createElement('small'); li.dataset.eventId = event.id;
            const name = { command: 'Commande confirmée', pulse: `Activation pendant ${seconds(event.duration_ms)}`, expired: 'Arrêt automatique', configured: 'Configuration appliquée', removed: 'Broche retirée', startup: 'État au démarrage', input_changed: 'Niveau modifié', override: `Priorité manuelle ${seconds(event.duration_ms)}`, resumed: 'Priorité libérée' }[event.kind] || event.kind;
            label.textContent = `${name}${event.kind === 'command' || event.kind === 'pulse' || event.kind === 'expired' ? ' · ' + (event.active ? 'ON' : 'OFF') : ''}`;
            detail.textContent = `${new Date(event.at).toLocaleTimeString('fr-CA')} · ${sources[event.source] || event.source}${event.scene ? ' · ' + event.scene : ''}`;
            li.append(label, detail); $('pinEvents').append(li);
        });
        $('eventsEmpty').hidden = !!visible.length; $('eventStatus').textContent = journal;
        renderHistory();
        const key = `gpio${gpio}`, hasHA = HALinks.available(config) && !!config.home_assistant.entities[key]; $('pinHALinks').hidden = !hasHA;
        HALinks.link($('pinHA24'), HALinks.history(config, key, 24)); HALinks.link($('pinHA7'), HALinks.history(config, key, 168)); HALinks.link($('pinHAEntity'), HALinks.entity(config, key));
    }
    async function refresh() {
        const started = epoch;
        const results = await Promise.allSettled([U.json('/api/status'), U.json('/api/io/config'), U.json('/api/io/status'), U.json('/api/io/automation/status'), U.json(`/api/io/events?after=${cursor}&session=${encodeURIComponent(session)}`)]);
        if (started !== epoch) return;
        const [status, settings, values, auto, history] = results;
        if (settings.status === 'fulfilled') config = settings.value;
        if (values.status === 'fulfilled') io = values.value;
        online = [status, settings, values].every(r => r.status === 'fulfilled') && (!io?._revision || io._revision === config?._revision);
        automation = auto.status === 'fulfilled' && (!auto.value._revision || auto.value._revision === config?._revision) ? auto.value : null;
        if (online) { lastSuccess = Date.now(); U.badge('boardBadge', 'Board en ligne', 'ok'); } else U.badge('boardBadge', status.status === 'fulfilled' ? 'Synchronisation en cours' : 'Board inaccessible', 'error');
        $('lastUpdated').textContent = online ? 'Valeur confirmée à l’instant' : lastSuccess ? `Dernière réponse ${U.age(Date.now() - lastSuccess)}` : 'En attente';
        if (history.status === 'fulfilled' && Array.isArray(history.value.events) && typeof history.value.session === 'string') {
            const h = history.value; if (h.reset || session && session !== h.session) { events = []; cursor = 0; lost = 0; samples = []; since = 0; historyNote = session ? 'Redémarrage : nouvelle série.' : ''; }
            if (h.lost) { events = []; samples = []; since = 0; historyNote = 'Journal incomplet : série limitée aux événements encore disponibles.'; }
            session = h.session; lost += h.lost || 0;
            h.events.filter(e => e.id > cursor).forEach(e => events.push({ ...e, at: Date.now() - ((h.now_ms - e.at_ms) >>> 0) })); cursor = h.latest_id; events = events.slice(-64);
            journalOnline = true;
            journal = `Journal en ligne${lost ? ` · ${lost} événement(s) remplacé(s) pendant votre absence` : ''}`;
        } else { journalOnline = false; journal = 'Journal inaccessible · derniers événements reçus conservés, réessai automatique.'; }
        for (const option of $('detailGPIO').options) if (option.value) { const name = config?.io_pins?.find(p => p.gpio === Number(option.value))?.label; option.textContent = `GPIO ${option.value}${name ? ' · ' + name : ''}`; }
        observe(); render();
    }
    async function command(path, message) {
        if (!known() || !M.output(pin()) || busy) return; ++epoch; busy = true; render(); U.notice('Commande en cours…');
        try { await U.json(path, { method: 'POST' }); ++epoch; await refresh(); if (!known()) throw new Error('Valeur actuelle non confirmée'); U.notice(message); }
        catch (e) { online = false; U.notice(`Commande non confirmée : ${e.message}`, true); }
        finally { busy = false; render(); }
    }
    $('pinOn').addEventListener('click', () => command(`/api/io/set?gpio=${gpio}&active=1`, 'Commande ON acceptée. La valeur affichée est confirmée par le board.'));
    $('pinOff').addEventListener('click', () => command(`/api/io/set?gpio=${gpio}&active=0`, 'Commande OFF acceptée. La valeur affichée est confirmée par le board.'));
    $('pinTest').addEventListener('click', () => command(`/api/io/set?gpio=${gpio}&active=1&duration_ms=1000`, 'Impulsion confirmée. Le board assure le retour OFF après 1 s.'));
    $('pinPriority').addEventListener('click', () => command(`/api/io/override?gpio=${gpio}&duration_ms=${reading()?.override_remaining_ms ? 0 : $('priorityDuration').value}`, reading()?.override_remaining_ms ? 'Priorité libérée. La pause globale éventuelle reste active.' : 'Priorité manuelle confirmée.'));
    $('detailGPIO').addEventListener('change', () => { ++epoch; gpio = $('detailGPIO').value === '' ? null : Number($('detailGPIO').value); samples = []; seriesKey = ''; since = 0; historyNote = ''; const url = new URL(location.href); if (gpio === null) url.searchParams.delete('gpio'); else url.searchParams.set('gpio', gpio); history.replaceState(null, '', url); U.notice(''); observe(); render(); });
    $('historyExport').addEventListener('click', () => U.download(`gpio-${gpio}-recent.csv`, 'date_iso,valeur,origine\n' + historyRows().map(r => `${new Date(r.t).toISOString()},${r.value ?? ''},${r.origin}`).join('\n'), 'text/csv;charset=utf-8'));
    render(); const poll = U.poll(refresh, 2000); $('refreshButton').addEventListener('click', poll);
})();
