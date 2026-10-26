(() => {
    const U = BoardUI;
    const M = IOModel, { GPIOS, INPUT_ONLY, ADC2, ADC } = M;
    const MODE_NAMES = { INPUT: 'Entrée', INPUT_PULLUP: 'Entrée · pull-up ↑', INPUT_PULLDOWN: 'Entrée · pull-down ↓', OUTPUT: 'Sortie' };
    const clone = value => JSON.parse(JSON.stringify(value));
    const signature = M.signature;
    const byId = id => document.getElementById(id);
    const FILTERS = ['all', 'configured', 'favorites', 'output', 'input', 'analog', 'changed'];
    const preferenceKey = 'iyak32.io.preferences';
    let preferences = {};
    try { preferences = JSON.parse(localStorage.getItem(preferenceKey) || '{}') || {}; } catch (_) { /* Preferences are optional. */ }
    const favorites = new Set(Array.isArray(preferences.favorites) ? preferences.favorites.filter(gpio => Number.isInteger(gpio) && GPIOS.includes(gpio)) : []);
    let savedDocument = { io_pins: [] }, saved = new Map(), draft = new Map(), values = new Map();
    let draftAlerts = null, alertConflict = false;
    let draftAutomation = null, automationConflict = false, draftDevice = null, deviceConflict = false;
    let draftScenes = [], sceneConflict = false, editingScene = '', sceneBusy = false, pendingImport = null;
    let loaded = false, online = false, saving = false, filter = FILTERS.includes(preferences.filter) ? preferences.filter : 'all', lastSuccess = 0, commandEpoch = 0;
    const rows = new Map(), editors = new Map(), pending = new Set(), samples = new Map();
    const cards = new Map(), activity = [], conflicts = new Set();
    let eventSession = '', eventCursor = 0, eventRequest = Promise.resolve(), hiddenThrough = 0;
    let journalMessage = 'Connexion au journal…', journalLost = 0;
    const SOURCE_NAMES = { web: 'Web', mqtt: 'MQTT', button: 'Bouton', timer: 'Minuterie', config: 'Configuration', boot: 'Démarrage', input: 'Entrée', schedule: 'Horaire', rule: 'Règle', error: 'Cette page' };
    let pinMetadata = new Map();
    const active = pin => typeof pin?.active === "boolean" ? pin.active : !!pin?.value !== !!pin?.inverted;
    const startup = pin => pin.initial_state ? (!!(pin.initial_state === "HIGH") !== !!pin.inverted ? "ON" : "OFF") : "OFF";
    const changed = gpio => signature(saved.get(gpio)) !== signature(draft.get(gpio));
    const scenesChanged = () => signature(draftScenes) !== signature(savedDocument.scenes || []);
    const automationChanged = () => signature(draftAutomation) !== signature(savedDocument.automation ?? null);
    const alertsChanged = () => signature(draftAlerts) !== signature(savedDocument.alerts ?? null);
    const deviceChanged = () => signature(draftDevice) !== signature(savedDocument.device ?? null);
    const devicePreview = (list, value) => { const li = document.createElement('li'); li.textContent = `Nom du board : ${savedDocument.device?.name || 'identifiant du board'} → ${value?.name || 'identifiant du board'} · icône ${value?.icon || 'auto'}`; list.append(li); };
    const changeCount = () => GPIOS.filter(changed).length + Number(scenesChanged()) + Number(automationChanged()) + Number(deviceChanged()) + Number(alertsChanged());
    const hasConflicts = () => conflicts.size > 0 || sceneConflict || automationConflict || deviceConflict || alertConflict;
    const isOutput = M.output;
    const formatDuration = ms => ms >= 60000 && ms % 60000 === 0 ? `${ms / 60000} min` : `${ms / 1000} s`;
    const defaultPin = gpio => ({ gpio, label: `GPIO ${gpio}`, mode: 'INPUT', type: 'DIGITAL', initial_state: '', graph: false, inverted: false });

    function rememberPreferences() {
        try { localStorage.setItem(preferenceKey, JSON.stringify({ favorites: [...favorites], filter, search: byId('pinSearch').value })); } catch (_) { /* Keep working if storage is unavailable. */ }
    }
    function updateFavorite(button, gpio) {
        const favorite = favorites.has(gpio);
        button.textContent = favorite ? '★' : '☆'; button.setAttribute('aria-pressed', String(favorite));
        button.setAttribute('aria-label', `${favorite ? 'Retirer' : 'Ajouter'} GPIO ${gpio} ${favorite ? 'des' : 'aux'} favoris`);
        button.title = favorite ? 'Retirer des favoris' : 'Ajouter aux favoris';
    }
    function bindFavorite(button, gpio) {
        button.addEventListener('click', () => {
            if (favorites.has(gpio)) favorites.delete(gpio); else favorites.add(gpio);
            rememberPreferences(); updateAll();
        });
        updateFavorite(button, gpio);
    }
    function addFailure(gpio) {
        activity.unshift({ gpio, source: 'error', label: saved.get(gpio)?.label || `GPIO ${gpio}`, at: Date.now() });
        activity.splice(64); renderActivity();
    }
    function visibleEvents() {
        const source = byId('activitySource').value;
        return activity.filter(event => (event.id == null || event.id > hiddenThrough) && (!source || event.source === source));
    }
    function refreshEvents() {
        eventRequest = eventRequest.then(async () => {
            try {
                const result = await U.json(`/api/io/events?after=${eventCursor}&session=${encodeURIComponent(eventSession)}`);
                if (!Array.isArray(result.events) || typeof result.session !== 'string') throw new Error('Journal invalide');
                if (result.reset || eventSession && eventSession !== result.session) {
                    activity.length = 0; eventCursor = 0; hiddenThrough = 0; journalLost = 0;
                }
                eventSession = result.session;
                journalLost += result.lost || 0;
                result.events.forEach(event => {
                    if (event.id <= eventCursor) return;
                    activity.unshift({ ...event, at: Date.now() - ((result.now_ms - event.at_ms) >>> 0) });
                });
                eventCursor = result.latest_id;
                activity.splice(64);
                journalMessage = `Journal en ligne · ${result.capacity} événements maximum${journalLost ? ` · ${journalLost} événement(s) ancien(s) remplacé(s) pendant votre absence` : ''}`;
            } catch (_) { journalMessage = 'Journal inaccessible · réessai automatique'; }
            renderActivity();
        });
        return eventRequest;
    }
    async function refreshDiscovery() {
        try {
            const status = await U.json('/api/mqtt/status');
            U.badge('ioDiscoveryBadge', !status.connected ? 'MQTT déconnecté' : status.io_discovery_ready ? `${status.io_discovery_count} I/O annoncées à Home Assistant` : 'Annonce des I/O en cours…', status.connected && status.io_discovery_ready ? 'ok' : 'warning');
        } catch (_) { U.badge('ioDiscoveryBadge', 'État MQTT indisponible', 'warning'); }
    }
    function renderActivity() {
        const list = byId('activityList'); list.replaceChildren();
        const visible = visibleEvents();
        visible.forEach(event => {
            const li = document.createElement('li'); li.className = 'activity-event'; li.dataset.source = event.source;
            if (event.id != null) li.dataset.eventId = event.id;
            const time = document.createElement('time'); time.dateTime = new Date(event.at).toISOString(); time.textContent = new Date(event.at).toLocaleTimeString('fr-FR');
            const description = document.createElement('div'), name = document.createElement('strong'), detail = document.createElement('span');
            name.textContent = `${event.label} · GPIO ${event.gpio}`;
            const action = { command: 'Commande confirmée', pulse: `Activation pour ${event.duration_ms / 1000} s`, expired: 'Arrêt automatique', configured: 'Configuration appliquée', removed: 'Broche retirée', startup: 'État au démarrage', input_changed: 'Niveau modifié' };
            detail.textContent = `${SOURCE_NAMES[event.source] || event.source} · ${event.source === 'error' ? 'Commande non confirmée · local à cet onglet' : action[event.kind] || event.kind}${event.scene ? ` · Scène : ${event.scene}` : ''}`;
            description.append(name, detail);
            const state = document.createElement('span'); state.className = 'activity-state';
            const current = event.output ? Number(event.active) : event.value;
            const level = value => event.output ? value ? 'ON' : 'OFF' : value ? 'HAUT' : 'BAS';
            state.textContent = event.source === 'error' ? 'ÉCHEC' : event.kind === 'removed' ? 'RETIRÉE' : event.previous != null && event.previous !== current ? `${level(event.previous)} → ${level(current)}` : level(current);
            state.dataset.state = event.source === 'error' ? 'error' : current ? 'ok' : 'neutral';
            li.append(time, description, state); list.append(li);
        });
        byId('activityEmpty').hidden = visible.length > 0;
        byId('clearActivity').disabled = visible.length === 0; byId('exportActivity').disabled = visible.length === 0;
        byId('activityStatus').textContent = journalMessage;
    }

    function updateOutputCards() {
        const outputs = [...values.values()].filter(isOutput).sort((a, b) => Number(favorites.has(b.gpio)) - Number(favorites.has(a.gpio)) || a.gpio - b.gpio), container = byId('outputCards');
        const ids = new Set(outputs.map(pin => pin.gpio));
        cards.forEach((card, gpio) => { if (!ids.has(gpio)) { card.remove(); cards.delete(gpio); } });
        outputs.forEach(pin => {
            let card = cards.get(pin.gpio);
            if (!card) {
                card = document.createElement('article'); card.className = 'panel io-output-card'; card.dataset.gpio = pin.gpio;
                card.innerHTML = '<div class="output-card-heading"><div><strong class="output-card-name"></strong><span class="output-card-gpio"></span></div><button class="favorite-button" type="button"></button></div><div class="output-card-reading"><strong class="output-card-state">—</strong><span class="output-card-level"></span></div><div class="output-card-actions"><div class="pin-commands"><button type="button" data-value="1" aria-pressed="false">ON</button><button type="button" data-value="0" aria-pressed="false">OFF</button></div><button class="output-card-config" type="button">Configurer →</button></div><div class="output-pulse"><label>Durée <input type="number" min="1" max="3600" step="1" value="10" aria-label="Durée en secondes"> s</label><button class="btn secondary pulse-button" type="button">Activer pour cette durée</button></div><p class="output-countdown" aria-live="off"></p><p class="output-card-note"></p>';
                const shortcuts = document.createElement('div'); shortcuts.className = 'pulse-presets'; shortcuts.setAttribute('aria-label', `Durées favorites GPIO ${pin.gpio}`);
                card.querySelector('.output-pulse').before(shortcuts);
                bindFavorite(card.querySelector('.favorite-button'), pin.gpio);
                card.querySelector('.output-card-gpio').textContent = `GPIO ${pin.gpio}`;
                card.querySelectorAll('[data-value]').forEach(button => {
                    button.setAttribute('aria-label', `GPIO ${pin.gpio} ${button.textContent}`);
                    button.addEventListener('click', () => command(pin.gpio, Number(button.dataset.value)));
                });
                card.querySelector('.output-pulse input').setAttribute('aria-label', `Durée en secondes GPIO ${pin.gpio}`);
                card.querySelector('.pulse-button').setAttribute('aria-label', `Activer GPIO ${pin.gpio} pour une durée`);
                card.querySelector('.pulse-button').addEventListener('click', () => {
                    const input = card.querySelector('.output-pulse input');
                    if (!input.reportValidity()) return;
                    command(pin.gpio, 1, Number(input.value) * 1000);
                });
                card.querySelector('.output-card-config').addEventListener('click', () => {
                    byId('pinSearch').value = ''; selectFilter('all');
                    if (rows.get(pin.gpio).querySelector('.edit-button').getAttribute('aria-expanded') !== 'true') toggleEditor(pin.gpio);
                    rows.get(pin.gpio).scrollIntoView({ behavior: 'smooth', block: 'center' });
                    editors.get(pin.gpio).querySelector('[data-field="label"]').focus({ preventScroll: true });
                });
                cards.set(pin.gpio, card); container.append(card);
            }
            const hasValue = online && (pin.value === 0 || pin.value === 1), modified = changed(pin.gpio);
            const shortcuts = card.querySelector('.pulse-presets'), presets = saved.get(pin.gpio)?.pulse_presets_ms ?? [5000, 30000, 60000];
            if (shortcuts.dataset.presets !== signature(presets)) {
                shortcuts.replaceChildren(); shortcuts.dataset.presets = signature(presets);
                presets.forEach(ms => {
                    const button = document.createElement('button'); button.className = 'btn secondary'; button.type = 'button'; button.dataset.duration = ms;
                    button.textContent = formatDuration(ms); button.setAttribute('aria-label', `Activer GPIO ${pin.gpio} pour ${formatDuration(ms)}`);
                    button.addEventListener('click', () => command(pin.gpio, 1, ms)); shortcuts.append(button);
                });
            }
            shortcuts.querySelectorAll('button').forEach(button => { button.disabled = !hasValue || modified || saving || sceneBusy || pending.has(pin.gpio); });
            card.querySelector('.output-card-name').textContent = saved.get(pin.gpio)?.label || pin.label;
            card.querySelector('.output-card-state').textContent = hasValue ? active(pin) ? 'ON' : 'OFF' : '—';
            card.dataset.on = hasValue ? String(active(pin)) : 'unknown';
            card.querySelector('.output-card-level').textContent = hasValue ? `${pin.value ? 'HIGH' : 'LOW'} · ${pin.value}` : pin.reason || 'État indisponible';
            card.querySelector('.output-card-note').textContent = pending.has(pin.gpio) ? 'Commande en cours…' : pin.value_error === 'PIN_IN_USE' ? `Sortie indisponible · utilisée par ${pin.reason}` : modified ? 'Configuration modifiée · enregistrement en attente' : `Au démarrage : ${startup(saved.get(pin.gpio) || pin)}${pin.inverted ? ' · ON = LOW' : ''}`;
            card.querySelector('.output-card-config').disabled = saving || pending.size > 0;
            card.querySelectorAll('[data-value]').forEach(button => {
                button.disabled = !hasValue || modified || saving || sceneBusy || pending.has(pin.gpio);
                button.setAttribute('aria-pressed', String(hasValue && Number(active(pin)) === Number(button.dataset.value)));
            });
            card.querySelectorAll('.output-pulse input, .pulse-button').forEach(control => { control.disabled = !hasValue || modified || saving || sceneBusy || pending.has(pin.gpio); });
            updateFavorite(card.querySelector('.favorite-button'), pin.gpio);
        });
        const focused = document.activeElement;
        outputs.forEach((pin, index) => {
            const card = cards.get(pin.gpio);
            if (container.children[index] !== card) container.insertBefore(card, container.children[index] || null);
        });
        if (focused && container.contains(focused) && document.activeElement !== focused) focused.focus({ preventScroll: true });
        byId('outputSection').hidden = outputs.length === 0;
        updateCountdowns();
    }
    function updateChangePreview() {
        const list = byId('changeList'); list.replaceChildren();
        GPIOS.filter(changed).forEach(gpio => {
            const before = saved.get(gpio), after = draft.get(gpio), changes = [];
            if (!before) changes.push(`Ajouter : ${after.label} · ${MODE_NAMES[after.mode || 'INPUT']}`);
            else if (!after) changes.push(`Retirer : ${before.label}`);
            else {
                if (before.label !== after.label) changes.push(`Nom : ${before.label} → ${after.label || '(vide)'}`);
                if ((before.mode || 'INPUT') !== after.mode) changes.push(`Mode : ${MODE_NAMES[before.mode || 'INPUT']} → ${MODE_NAMES[after.mode]}`);
                if ((before.type || 'DIGITAL') !== after.type) changes.push(`Type : ${before.type === 'ANALOG_INPUT' ? 'Analogique' : 'Numérique'} → ${after.type === 'ANALOG_INPUT' ? 'Analogique' : 'Numérique'}`);
                if (startup(before) !== startup(after)) changes.push(`Au démarrage : ${startup(before)} → ${startup(after)}`);
                if (!!before.inverted !== !!after.inverted) changes.push(`Polarité : ${after.inverted ? 'ON = LOW' : 'ON = HIGH'}`);
                if ((before.zone || '') !== (after.zone || '')) changes.push(`Zone : ${before.zone || 'aucune'} → ${after.zone || 'aucune'}`);
                if (!!before.graph !== !!after.graph) changes.push(`Suivi de la valeur : ${after.graph ? 'activé' : 'désactivé'}`);
                if (signature(before.pulse_presets_ms) !== signature(after.pulse_presets_ms)) changes.push(`Durées favorites : ${(after.pulse_presets_ms ?? [5000, 30000, 60000]).map(formatDuration).join(', ') || 'aucune'}`);
                if (signature(before.calibration) !== signature(after.calibration)) changes.push(after.calibration ? `Calibration : ${after.calibration.raw_min} → ${after.calibration.value_min} ${after.calibration.unit} ; ${after.calibration.raw_max} → ${after.calibration.value_max} ${after.calibration.unit}` : 'Calibration retirée');
            }
            const li = document.createElement('li'); li.textContent = `GPIO ${gpio} · ${changes.join(' ; ') || 'Configuration modifiée'}`; list.append(li);
        });
        if (deviceChanged()) devicePreview(list, draftDevice);
        if (alertsChanged()) { const li = document.createElement('li'); li.textContent = `Alertes : ${(savedDocument.alerts || []).length} → ${(draftAlerts || []).length}`; list.append(li); }
        if (automationChanged()) automationPreview(list, draftAutomation);
        const old = new Map((savedDocument.scenes || []).map(scene => [scene.id, scene])), next = new Map(draftScenes.map(scene => [scene.id, scene]));
        new Set([...old.keys(), ...next.keys()]).forEach(id => {
            if (signature(old.get(id)) === signature(next.get(id))) return;
            const li = document.createElement('li'); li.textContent = `Scène ${next.get(id)?.name || old.get(id).name} · ${!next.has(id) ? 'Retirer' : !old.has(id) ? 'Ajouter' : 'Modifier'}${next.has(id) ? ` · ${next.get(id).actions.length} sortie(s)` : ''}`; list.append(li);
        });
    }

    function automationPreview(list, settings) {
        const append = text => { const li = document.createElement('li'); li.textContent = text; list.append(li); };
        if (!settings) { append('Automatismes : aucune définition'); return; }
        append(`Automatismes : ${settings.paused ? 'en pause' : 'actifs'} · ${settings.timezone}`);
        const dayNames = ['Dim', 'Lun', 'Mar', 'Mer', 'Jeu', 'Ven', 'Sam'];
        (settings.schedules || []).forEach(item => append(`Horaire ${item.name} · ${item.enabled ? 'activé' : 'désactivé'} · ${item.days.map(day => dayNames[day]).join(', ')} ${item.time} → scène ${item.scene_id}`));
        (settings.rules || []).forEach(item => append(`Règle ${item.name} · ${item.enabled ? 'activée' : 'désactivée'} · GPIO ${item.gpio} ${['below', 'above'].includes(item.operator) ? `${item.operator === 'below' ? '<' : '>'} ${item.threshold} · marge ${item.hysteresis}` : item.operator === 'high' ? '= HIGH' : '= LOW'} · stable ${item.stable_ms / 1000} s · délai ${item.cooldown_ms / 1000} s → scène ${item.scene_id}`));
    }

    function payload() {
        const order = [...saved.keys(), ...GPIOS.filter(gpio => !saved.has(gpio))];
        return { ...savedDocument, ...(draftAlerts != null || savedDocument.alerts != null ? { alerts: clone(draftAlerts) } : {}), ...(draftDevice != null ? { device: clone(draftDevice) } : {}), ...(draftAutomation != null ? { automation: clone(draftAutomation) } : {}), io_pins: order.filter(gpio => draft.has(gpio)).map(gpio => clone(draft.get(gpio))), ...(draftScenes.length || savedDocument.scenes ? { scenes: clone(draftScenes) } : {}) };
    }
    function makeRows() {
        byId('pinRows').replaceChildren(); rows.clear(); editors.clear();
        const order = [...GPIOS].sort((a, b) => Number(saved.has(b)) - Number(saved.has(a)) || a - b);
        order.forEach(gpio => {
            const row = document.createElement('tr'); row.className = 'pin-row'; row.dataset.gpio = gpio;
            row.innerHTML = '<td class="pin-gpio"></td><td><div class="pin-name-line"><button class="favorite-button" type="button"></button><span class="pin-name"></span></div><span class="pin-meta"></span></td><td><span class="pin-config"></span><span class="pin-config-detail"></span><span class="pin-usage"></span></td><td><span class="pin-state"></span><span class="pin-value-detail"></span></td><td><div class="pin-commands"><button type="button" data-value="1" aria-pressed="false">ON</button><button type="button" data-value="0" aria-pressed="false">OFF</button></div><span class="pin-no-command muted">—</span></td><td><button class="btn secondary edit-button" type="button" aria-expanded="false">Configurer</button></td>';
            row.querySelector('.pin-gpio').textContent = `GPIO ${gpio}`;
            const detail = document.createElement('a'); detail.className = 'io-detail-link'; detail.href = `/io_detail.html?gpio=${gpio}`; detail.textContent = 'Fiche →'; detail.setAttribute('aria-label', `Fiche GPIO ${gpio}`); row.querySelector('.pin-gpio').append(detail);
            bindFavorite(row.querySelector('.favorite-button'), gpio);
            row.querySelector('.edit-button').setAttribute('aria-label', `Configurer GPIO ${gpio}`);
            row.querySelector('.edit-button').addEventListener('click', () => toggleEditor(gpio));
            row.querySelectorAll('[data-value]').forEach(button => {
                button.setAttribute('aria-label', `GPIO ${gpio} ${button.textContent}`);
                button.addEventListener('click', () => command(gpio, Number(button.dataset.value)));
            });
            const editor = document.createElement('tr'); editor.className = 'io-editor'; editor.hidden = true;
            editor.id = `editor-${gpio}`; row.querySelector('.edit-button').setAttribute('aria-controls', editor.id);
            byId('pinRows').append(row, editor); rows.set(gpio, row); editors.set(gpio, editor);
        });
    }
    function updateRow(gpio) {
        const row = rows.get(gpio), pin = draft.get(gpio), reading = values.get(gpio);
        const modified = changed(gpio), output = isOutput(reading), hasValue = online && reading && typeof reading.value === 'number';
        row.dataset.configured = String(!!pin); row.dataset.changed = String(modified);
        updateFavorite(row.querySelector('.favorite-button'), gpio);
        row.querySelector('.pin-name').textContent = pin?.label || saved.get(gpio)?.label || 'Non configurée';
        const meta = row.querySelector('.pin-meta');
        meta.textContent = modified ? pin ? 'Modification en attente' : 'Retrait en attente' : pin ? 'Configurée' : 'Non configurée';
        if (pin?.zone) meta.textContent += ` · ${pin.zone}`;
        meta.dataset.state = modified ? 'warning' : 'neutral';
        row.querySelector('.pin-config').textContent = pin ? `${MODE_NAMES[pin.mode || 'INPUT']} · ${pin.type === 'ANALOG_INPUT' ? 'Analogique' : 'Numérique'}` : 'Aucun rôle';
        const detail = row.querySelector('.pin-config-detail'); detail.dataset.state = 'neutral';
        detail.textContent = isOutput(pin) ? `Au démarrage : ${startup(pin)}${pin.inverted ? ' · ON = LOW' : ''}` : INPUT_ONLY.includes(gpio) ? 'Entrée uniquement · sans pull-up/down' : '';
        if (online && reading && pin && reading.mode !== pin.mode && !modified) {
            detail.textContent = `Mode effectif : ${MODE_NAMES[reading.mode] || reading.mode}`; detail.dataset.state = 'warning';
        }
        const state = row.querySelector('.pin-state'); state.dataset.on = hasValue ? String(output ? active(reading) : !!reading.value) : 'unknown';
        const calibrated = reading?.type === 'ANALOG_INPUT' && reading.calibration;
        state.textContent = hasValue ? reading.type === 'ANALOG_INPUT' ? calibrated ? `${U.number(typeof reading.calibrated_value === 'number' ? reading.calibrated_value : M.convert(reading.value, calibrated), 1)} ${calibrated.unit}`.trim() : U.number(reading.value, 0) : output ? active(reading) ? 'ON' : 'OFF' : reading.value ? 'HAUT' : 'BAS' : '—';
        if (hasValue && reading.type !== 'ANALOG_INPUT') { const span = document.createElement('span'); span.className = 'pin-level'; span.textContent = state.textContent; state.replaceChildren(span); }
        row.querySelector('.pin-value-detail').textContent = hasValue ? reading.type === 'ANALOG_INPUT' ? `ADC brut : ${reading.value} · 0–4095` : `${reading.value} · ${reading.value ? 'HIGH' : 'LOW'}`
            : !online && saved.has(gpio) ? 'Board inaccessible' : reading?.value_error === 'PIN_IN_USE' ? `Utilisée par ${reading.reason}` : reading?.value_error === 'ADC2_WIFI' ? 'ADC2 : Wi-Fi actif' : !reading ? 'Non lue' : 'Indisponible';
        row.querySelector('.pin-commands').hidden = !output;
        const usage = row.querySelector('.pin-usage'), usages = pinMetadata.get(gpio)?.usages || [];
        usage.textContent = usages.map(owner => owner.label).join(' · ');
        const conflicting = usages.some(owner => ['I2C', 'SERIAL'].includes(owner.mode) && pin || pin && owner.mode === 'ANALOG_INPUT' && pin.mode === 'OUTPUT' || pin && owner.mode === 'OUTPUT' && !isOutput(pin));
        usage.dataset.state = conflicting ? 'warning' : 'neutral';
        if (conflicting) usage.textContent = '⚠ Usage partagé : ' + usage.textContent;
        if (conflicts.has(gpio)) { meta.textContent = 'Modifiée ailleurs · à vérifier'; meta.dataset.state = 'warning'; }
        row.querySelector('.pin-no-command').hidden = !!output;
        row.querySelectorAll('[data-value]').forEach(button => {
            button.disabled = !hasValue || !output || modified || saving || sceneBusy || pending.has(gpio);
            button.setAttribute('aria-pressed', String(hasValue && Number(active(reading)) === Number(button.dataset.value)));
        });
        row.querySelector('.edit-button').disabled = !loaded || saving || pending.size > 0;
    }
    function applyFilters() {
        const query = byId('pinSearch').value.trim().toLowerCase(); let visible = 0;
        rows.forEach((row, gpio) => {
            const pin = draft.get(gpio), name = pin?.label || saved.get(gpio)?.label || '';
            const matches = !query || `${gpio} gpio ${gpio} ${name} ${pin?.zone || ''} ${(pinMetadata.get(gpio)?.usages || []).map(owner => owner.label).join(" ")}`.toLowerCase().includes(query);
            const matchesFilter = filter === 'all' || filter === 'configured' && !!pin || filter === 'output' && isOutput(pin)
                || filter === 'favorites' && favorites.has(gpio) || filter === 'input' && pin && !isOutput(pin) || filter === 'analog' && pin?.type === 'ANALOG_INPUT' || filter === 'changed' && changed(gpio);
            row.hidden = !(matches && matchesFilter && (!byId('zoneFilter').value || pin?.zone === byId('zoneFilter').value)); if (!row.hidden) visible++;
            const editor = editors.get(gpio); editor.hidden = row.hidden || row.querySelector('.edit-button').getAttribute('aria-expanded') !== 'true';
        });
        byId('visibleCount').textContent = `${visible} / ${GPIOS.length} GPIO affichés`;
        byId('emptyState').hidden = visible > 0;
        byId('emptyState').textContent = loaded ? filter === 'favorites' && favorites.size === 0 ? 'Touchez ☆ pour garder vos GPIO fréquents dans les favoris.' : 'Aucune broche ne correspond à ce filtre.' : 'Chargement des GPIO…';
    }
    function updateSummary() {
        const pins = [...saved.values()], outputs = [...values.values()].filter(isOutput), analog = pins.filter(pin => pin.type === 'ANALOG_INPUT').length;
        U.text('configuredCount', loaded ? pins.length : '—'); U.text('outputCount', loaded ? pins.filter(isOutput).length : '—');
        const unavailable = outputs.filter(pin => pin.value !== 0 && pin.value !== 1).length;
        U.text('outputDetail', online ? `${outputs.filter(pin => (pin.value === 0 || pin.value === 1) && active(pin)).length} ON · ${outputs.filter(pin => (pin.value === 0 || pin.value === 1) && !active(pin)).length} OFF${unavailable ? ` · ${unavailable} indisponible(s)` : ''}` : 'Valeurs indisponibles');
        U.text('inputCount', loaded ? pins.filter(pin => !isOutput(pin)).length : '—'); U.text('inputDetail', `${analog} analogique${analog > 1 ? 's' : ''}`);
        conflicts.forEach(gpio => { if (!changed(gpio)) conflicts.delete(gpio); });
        const count = changeCount(); byId('saveBar').hidden = count === 0;
        const pinCount = GPIOS.filter(changed).length;
        U.text('changeCount', `${pinCount} GPIO modifié(s)${scenesChanged() ? ' · scènes modifiées' : ''}${automationChanged() ? ' · automatismes modifiés' : ''}`);
        const error = M.validate(payload());
        byId('draftError').hidden = !error; byId('draftError').textContent = error;
        byId('saveButton').disabled = !count || !online || saving || pending.size > 0 || hasConflicts() || !!error;
        byId('discardButton').disabled = saving || pending.size > 0;
        byId('exportButton').disabled = !loaded || saving;
        byId('importButton').disabled = !loaded || !online || saving || pending.size > 0;
        byId('stageImport').disabled = saving || pending.size > 0;
        U.text('favoriteCount', favorites.size);
        byId('conflictBar').hidden = !hasConflicts();
        U.text('conflictMessage', `${conflicts.size} GPIO${sceneConflict ? ' et les scènes' : ''}${automationConflict ? ' et les automatismes' : ''}${deviceConflict ? ' et le nom du board' : ''}${alertConflict ? ' et les alertes' : ''} modifié(s) ailleurs et dans vos brouillons. Vérifiez le comparatif avant d’enregistrer.`);
    }
    function updateAll() {
        const zones = [...new Set([...draft.values()].map(pin => pin.zone).filter(Boolean))].sort(), zoneValue = byId('zoneFilter').value;
        byId('zoneFilter').replaceChildren(new Option('Toutes les zones', '')); zones.forEach(zone => byId('zoneFilter').add(new Option(zone, zone))); byId('zoneFilter').value = zones.includes(zoneValue) ? zoneValue : '';
        byId('zoneNames').replaceChildren(...zones.map(zone => new Option(zone, zone)));
        rows.forEach((_, gpio) => updateRow(gpio)); updateSummary(); applyFilters(); updateOutputCards(); updateChangePreview(); updateScenes(); updateCalibrationPreviews(); }

    function sceneEditorKey(scene) { return signature({ scene, busy: saving || pending.size > 0, pins: [...draft.values()].map(pin => ({ gpio: pin.gpio, label: pin.label, output: isOutput(pin) })) }); }
    function updateScenes() {
        const available = [...values.values()].filter(pin => isOutput(pin) && (pin.value === 0 || pin.value === 1));
        const unavailable = [...values.values()].filter(pin => isOutput(pin) && pin.value == null);
        byId('allOff').disabled = !online || saving || pending.size > 0 || available.length === 0;
        byId('newScene').disabled = !loaded || saving || pending.size > 0 || draftScenes.length >= 12;
        byId('allOffNote').textContent = `Tout OFF arrête ${available.length} sortie(s) disponible(s) et leurs minuteries${unavailable.length ? ` · GPIO ${unavailable.map(pin => pin.gpio).join(', ')} indisponible(s), ignoré(s)` : ''}.`;
        const container = byId('sceneCards');
        const key = signature({ scenes: draftScenes, changed: scenesChanged(), pins: [...draft.values()].map(pin => [pin.gpio, pin.label, changed(pin.gpio), values.get(pin.gpio)?.value == null]), online, saving, pending: [...pending] });
        if (container.dataset.key !== key) {
            container.dataset.key = key; container.replaceChildren();
            draftScenes.forEach(scene => {
                const card = document.createElement('article'); card.className = 'scene-card'; card.dataset.sceneId = scene.id; if (scene.id === new URLSearchParams(location.search).get('scene')) card.classList.add('linked-item');
                const title = document.createElement('h3'); title.textContent = IOModel.icon(scene) + ' ' + (scene.name || 'Nouvelle scène') + (scene.zone ? ' · ' + scene.zone : '');
                const details = document.createElement('ul');
                scene.actions.forEach(action => {
                    const li = document.createElement('li'); li.textContent = `${draft.get(action.gpio)?.label || `GPIO ${action.gpio}`} · ${action.active ? 'ON' : 'OFF'}${action.duration_ms ? ` pour ${formatDuration(action.duration_ms)}` : ''}`; details.append(li);
                });
                const blocked = scene.actions.some(action => !isOutput(saved.get(action.gpio)) || values.get(action.gpio)?.value == null);
                const changedPins = scene.actions.some(action => changed(action.gpio));
                const note = document.createElement('p'); note.className = 'panel-note'; note.textContent = scenesChanged() || changedPins ? 'Brouillon · enregistrement en attente' : blocked ? 'Une sortie est indisponible' : 'Prête à lancer';
                const tools = document.createElement('div'); tools.className = 'toolbar';
                const run = document.createElement('button'); run.className = 'btn'; run.type = 'button'; run.textContent = 'Lancer';
                run.disabled = !online || saving || pending.size > 0 || scenesChanged() || changedPins || blocked || !scene.actions.length;
                run.addEventListener('click', () => runScene(scene.id));
                const edit = document.createElement('button'); edit.className = 'btn secondary'; edit.type = 'button'; edit.textContent = 'Modifier'; edit.disabled = saving || pending.size > 0;
                edit.addEventListener('click', () => { editingScene = scene.id; byId('sceneActions').dataset.key = ''; updateScenes(); byId('sceneName').focus(); });
                const duplicate = document.createElement('button'); duplicate.className = 'btn secondary'; duplicate.textContent = 'Dupliquer'; duplicate.disabled = saving || pending.size > 0 || draftScenes.length >= 12; duplicate.addEventListener('click', () => { const copy = { ...clone(scene), id: IOModel.newId('scene'), name: IOModel.copyName(scene.name) }; draftScenes.push(copy); editingScene = copy.id; updateAll(); U.notice('Scène copiée dans le brouillon.'); }); tools.append(run, edit, duplicate); card.append(title, details, note, tools); container.append(card);
            });
        }
        byId('scenesEmpty').hidden = draftScenes.length > 0;
        const scene = draftScenes.find(item => item.id === editingScene);
        byId('sceneEditor').hidden = !scene;
        if (!scene) return;
        byId('sceneIcon').value = scene.icon || ''; byId('sceneIcon').disabled = saving || pending.size > 0; byId('sceneName').value = scene.name; byId('sceneZone').value = scene.zone || ''; byId('sceneZone').disabled = saving || pending.size > 0;
        byId('sceneName').disabled = saving || pending.size > 0;
        byId('deleteScene').disabled = saving || pending.size > 0;
        const actions = byId('sceneActions'), formKey = sceneEditorKey(scene);
        if (actions.dataset.key !== formKey) {
            actions.dataset.key = formKey; actions.replaceChildren();
            const gpios = new Set([...draft.values()].filter(isOutput).map(pin => pin.gpio)); scene.actions.forEach(action => gpios.add(action.gpio));
            [...gpios].sort((a, b) => a - b).forEach(gpio => {
                const row = document.createElement('div'); row.className = 'scene-action'; row.dataset.gpio = gpio;
                row.innerHTML = '<label class="scene-target"><input type="checkbox" data-scene-field="enabled"><span></span></label><label>État<select data-scene-field="active"><option value="1">ON</option><option value="0">OFF</option></select></label><label>Durée (s)<input type="number" data-scene-field="duration" min="0.1" max="3600" step="0.1" placeholder="Continue"></label>';
                const action = scene.actions.find(item => item.gpio === gpio), pin = draft.get(gpio);
                const enabled = row.querySelector('[data-scene-field="enabled"]'), state = row.querySelector('select'), duration = row.querySelector('[type="number"]');
                row.querySelector('.scene-target span').textContent = `${pin?.label || `GPIO ${gpio}`} · GPIO ${gpio}${!isOutput(pin) ? ' · sortie retirée' : values.get(gpio)?.value_error === 'PIN_IN_USE' ? ' · indisponible' : ''}`;
                enabled.checked = !!action; state.value = action?.active === false ? '0' : '1'; duration.value = action?.duration_ms ? action.duration_ms / 1000 : '';
                const fields = () => {
                    enabled.disabled = saving || pending.size > 0 || !enabled.checked && (!isOutput(pin) || values.get(gpio)?.value_error === 'PIN_IN_USE');
                    state.disabled = saving || pending.size > 0 || !enabled.checked || !isOutput(pin);
                    duration.disabled = state.disabled || state.value === '0';
                };
                row.querySelectorAll('input, select').forEach(input => input.addEventListener(input.tagName === 'SELECT' || input.type === 'checkbox' ? 'change' : 'input', () => {
                    scene.actions = scene.actions.filter(item => item.gpio !== gpio);
                    if (enabled.checked) scene.actions.push({ gpio, active: state.value === '1', duration_ms: state.value === '1' && duration.value !== '' ? Math.round(Number(duration.value) * 1000) || -1 : 0 });
                    if (state.value === '0') duration.value = '';
                    fields(); actions.dataset.key = sceneEditorKey(scene); updateAll();
                }));
                fields(); actions.append(row);
            });
        }
        if (saving || pending.size > 0) actions.querySelectorAll('input, select').forEach(input => { input.disabled = true; });
    }
    async function runScene(id) {
        if (!online || saving || pending.size > 0 || sceneBusy) return;
        const scene = id === 'all_off' ? { id, name: 'Tout OFF', actions: [...values.values()].filter(pin => isOutput(pin) && pin.value != null).map(pin => ({ gpio: pin.gpio, active: false })) } : (savedDocument.scenes || []).find(scene => scene.id === id);
        if (!scene || id !== 'all_off' && (scenesChanged() || scene.actions.some(action => changed(action.gpio)))) return;
        commandEpoch++; sceneBusy = true; scene.actions.forEach(action => pending.add(action.gpio)); updateAll();
        try {
            const result = await U.json('/api/io/scene', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ id, _revision: savedDocument._revision }) });
            if (result.status !== 'success' || !Array.isArray(result.outputs)) throw new Error('La scène n’a pas été confirmée.');
            result.outputs.forEach(pin => values.set(pin.gpio, { ...values.get(pin.gpio), ...pin, _readAt: Date.now() }));
            U.notice(`${result.name} · ${result.outputs.length} sortie(s) confirmée(s)${result.skipped?.length ? ` · GPIO ${result.skipped.join(', ')} indisponible(s), ignoré(s)` : ''}.`);
            await refreshEvents();
        } catch (error) { online = false; U.notice(`${scene.name} : ${error.message}`, true); }
        finally { sceneBusy = false; pending.clear(); byId('sceneActions').dataset.key = ''; updateAll(); await refresh(); }
    }

    function updateCalibrationPreviews() {
        editors.forEach((editor, gpio) => {
            const preview = editor.querySelector('.calibration-preview'); if (!preview) return;
            const c = draft.get(gpio)?.calibration, reading = values.get(gpio), raw = reading?.type === 'ANALOG_INPUT' ? reading.value : null, error = M.calibrationError(c);
            preview.textContent = !c ? 'Activez la conversion pour prévisualiser une valeur utile.' : error || (!online ? 'Lecture ADC indisponible · aperçu en attente' : reading?.type !== 'ANALOG_INPUT' ? 'Enregistrez le mode analogique pour lire l’ADC et prévisualiser.' : typeof raw !== 'number' ? 'Lecture ADC indisponible · aperçu en attente' : `${raw} ADC → ${U.number(M.convert(raw, c), 1)} ${c.unit}`);
            preview.dataset.state = error ? 'error' : 'neutral';
            editor.querySelectorAll('[data-capture]').forEach(button => { button.disabled = saving || pending.size > 0 || !c || !online || typeof raw !== 'number'; });
        });
    }

    function renderImport() {
        if (!pendingImport) return;
        const next = new Map(pendingImport.io_pins.map(pin => [pin.gpio, pin])), list = byId('importChanges'); list.replaceChildren();
        let added = 0, modified = 0, removed = 0;
        new Set([...saved.keys(), ...next.keys()]).forEach(gpio => {
            if (signature(saved.get(gpio)) === signature(next.get(gpio))) return;
            const before = saved.get(gpio), after = next.get(gpio), li = document.createElement('li');
            if (!before) added++; else if (!after) removed++; else modified++;
            li.textContent = `GPIO ${gpio} · ${!before ? 'Ajouter' : !after ? 'Retirer' : 'Modifier'} : ${before?.label || ''}${before && after ? ' → ' : ''}${after?.label || ''}${after ? ` · zone ${after.zone || 'aucune'}` : ''}${after ? ` · ${MODE_NAMES[after.mode || 'INPUT']} · ${after.type === 'ANALOG_INPUT' ? 'Analogique' : 'Numérique'}${isOutput(after) ? ` · démarrage ${startup(after)} · ON = ${after.inverted ? 'LOW' : 'HIGH'}` : ''}${after.calibration ? ` · ADC ${after.calibration.raw_min} → ${after.calibration.value_min} ${after.calibration.unit} ; ADC ${after.calibration.raw_max} → ${after.calibration.value_max} ${after.calibration.unit}${after.calibration.clamp === false ? ' · extrapolation' : ' · plage limitée'}` : before?.calibration ? ' · calibration retirée' : ''}${after.pulse_presets_ms ? ` · durées : ${after.pulse_presets_ms.map(formatDuration).join(', ') || 'aucune'}` : ''}` : ''}`; list.append(li);
        });
        if (signature(pendingImport.alerts) !== signature(savedDocument.alerts ?? null)) { const li = document.createElement('li'); li.textContent = `Alertes : ${(savedDocument.alerts || []).length} → ${(pendingImport.alerts || []).length} · ${(pendingImport.alerts || []).map(a => a.name).join(', ') || 'aucune'}`; list.append(li); }
        if (signature(pendingImport.device) !== signature(savedDocument.device ?? null)) devicePreview(list, pendingImport.device);
        if (signature(pendingImport.automation) !== signature(savedDocument.automation ?? null)) automationPreview(list, pendingImport.automation);
        const importedScenes = pendingImport.scenes || [];
        const oldScenes = new Map((savedDocument.scenes || []).map(scene => [scene.id, scene])), nextScenes = new Map(importedScenes.map(scene => [scene.id, scene]));
        new Set([...oldScenes.keys(), ...nextScenes.keys()]).forEach(id => {
            if (signature(oldScenes.get(id)) === signature(nextScenes.get(id))) return;
            const scene = nextScenes.get(id), li = document.createElement('li');
            li.textContent = `Scène ${scene?.name || oldScenes.get(id).name} · ${!scene ? 'Retirer' : !oldScenes.has(id) ? 'Ajouter' : 'Modifier'}${scene ? ` · zone ${scene.zone || 'aucune'} · ${scene.actions.map(action => `GPIO ${action.gpio} ${action.active ? 'ON' : 'OFF'}${action.duration_ms ? ` (${formatDuration(action.duration_ms)})` : ''}`).join(' ; ')}` : ''}`; list.append(li);
        });
        byId('importSummary').textContent = `${added} ajout(s) · ${modified} modification(s) · ${removed} retrait(s)`;
        if (!list.childElementCount) { const li = document.createElement('li'); li.textContent = 'Configuration identique à celle du board.'; list.append(li); }
        byId('importPanel').hidden = false;
    }
    async function readImport(file) {
        if (!file) return;
        try {
            if (file.size > 65536) throw new Error('Le fichier dépasse 64 Ko.');
            const imported = JSON.parse(await file.text());
            const candidate = { ...savedDocument, ...(Object.hasOwn(imported, 'alerts') ? { alerts: imported.alerts } : {}), ...(imported.device != null ? { device: imported.device } : {}), io_pins: imported.io_pins, scenes: imported.scenes ?? [], ...(imported.automation != null ? { automation: imported.automation } : {}) };
            const error = M.validate(candidate); if (error) throw new Error(error);
            await U.json('/api/io/config/validate', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(candidate) });
            pendingImport = { alerts: clone(candidate.alerts ?? null), device: clone(candidate.device ?? null), io_pins: clone(candidate.io_pins), scenes: clone(candidate.scenes), automation: clone(candidate.automation ?? null) }; renderImport();
            byId('importPanel').scrollIntoView({ behavior: 'smooth', block: 'center' });
        } catch (error) { U.notice(`Import refusé : ${error.message}`, true); }
        finally { byId('importFile').value = ''; }
    }

    function toggleEditor(gpio) {
        const row = rows.get(gpio), button = row.querySelector('.edit-button'), editor = editors.get(gpio);
        const open = button.getAttribute('aria-expanded') !== 'true'; button.setAttribute('aria-expanded', String(open));
        button.textContent = open ? 'Fermer' : 'Configurer'; editor.hidden = !open;
        if (!open) return;
        const cell = document.createElement('td'); cell.colSpan = 6;
        cell.innerHTML = '<div class="pin-editor"><label class="pin-enabled"><input type="checkbox" data-field="enabled">Configurer cette broche</label><label>Nom<input type="text" data-field="label" maxlength="64"></label><label>Icône<select data-field="icon"></select></label><label>Zone<input type="text" data-field="zone" maxlength="64" list="zoneNames" placeholder="Bac 1, Éclairage…"></label><label>Mode<select data-field="mode"><option value="INPUT">Entrée</option><option value="INPUT_PULLUP">Entrée · pull-up</option><option value="INPUT_PULLDOWN">Entrée · pull-down</option><option value="OUTPUT">Sortie</option></select></label><label>Type<select data-field="type"><option value="DIGITAL">Numérique</option><option value="ANALOG_INPUT">Analogique (ADC)</option></select></label><label>État au démarrage<select data-field="initial_state"><option value="">OFF par défaut</option><option value="LOW">LOW (0)</option><option value="HIGH">HIGH (1)</option></select></label><label class="pin-inverted"><input type="checkbox" data-field="inverted">Sortie inversée · ON = LOW</label><label class="pin-graph"><input type="checkbox" data-field="graph">Suivre la valeur</label><label class="pin-presets">Durées favorites (s)<input type="text" data-field="pulse_presets_ms" placeholder="5 ; 30 ; 60"><small>Jusqu’à cinq durées, séparées par ;. Vide : aucun raccourci.</small></label><fieldset class="calibration-fields"><legend>Conversion analogique</legend><label class="calibration-toggle"><input type="checkbox" data-field="calibration_enabled">Afficher une valeur calibrée</label><div class="calibration-grid"><label>ADC référence A<input type="number" data-field="raw_min" min="0" max="4095" step="1"><button type="button" class="btn secondary" data-capture="raw_min">Prendre la lecture</button></label><label>Valeur A<input type="number" data-field="value_min" step="any"></label><label>ADC référence B<input type="number" data-field="raw_max" min="0" max="4095" step="1"><button type="button" class="btn secondary" data-capture="raw_max">Prendre la lecture</button></label><label>Valeur B<input type="number" data-field="value_max" step="any"></label><label>Unité<input type="text" data-field="unit" maxlength="16" placeholder="%, °C…"></label><label class="calibration-toggle"><input type="checkbox" data-field="clamp">Limiter à la plage</label></div><p class="calibration-preview" role="status"></p><p class="panel-note">Les références A et B peuvent être inversées. L’ADC brut reste visible et disponible via MQTT.</p></fieldset><p class="editor-help"></p></div>';
        editor.replaceChildren(cell);
        const fields = field => cell.querySelector(`[data-field="${field}"]`);
        let local = clone(draft.get(gpio) || defaultPin(gpio));
        let presetsEdited = local.pulse_presets_ms != null;
        fields('enabled').checked = draft.has(gpio);
        fields('icon').replaceChildren(...IOModel.iconOptions()); fields('icon').value = local.icon || ''; fields('label').value = local.label; fields('zone').value = local.zone || ''; fields('mode').value = local.mode || 'INPUT'; fields('type').value = local.type || 'DIGITAL';
        fields('initial_state').value = local.initial_state || ''; fields('inverted').checked = !!local.inverted; fields('graph').checked = !!local.graph;
        fields('pulse_presets_ms').value = (local.pulse_presets_ms ?? [5000, 30000, 60000]).map(ms => ms / 1000).join(' ; ');
        const c = local.calibration || { raw_min: 0, raw_max: 4095, value_min: 0, value_max: 100, unit: '%', clamp: true };
        fields('calibration_enabled').checked = !!local.calibration;
        ['raw_min', 'raw_max', 'value_min', 'value_max', 'unit'].forEach(key => { fields(key).value = c[key]; }); fields('clamp').checked = c.clamp !== false;
        const updateFields = () => {
            const enabled = fields('enabled').checked;
            cell.querySelectorAll('[data-field]').forEach(input => { input.disabled = saving || input.dataset.field !== 'enabled' && !enabled; input.setAttribute('aria-label', `GPIO ${gpio} ${input.closest('label').textContent.trim()}`); });
            fields('mode').querySelectorAll('option').forEach(option => { option.disabled = INPUT_ONLY.includes(gpio) && option.value !== 'INPUT'; });
            fields('type').querySelector('[value="ANALOG_INPUT"]').disabled = !ADC.includes(gpio) || fields('mode').value === 'OUTPUT';
            fields('initial_state').disabled = saving || !enabled || fields('mode').value !== 'OUTPUT';
            fields('inverted').disabled = saving || !enabled || fields('mode').value !== 'OUTPUT';
            cell.querySelector('.pin-presets').hidden = fields('mode').value !== 'OUTPUT';
            fields('pulse_presets_ms').disabled = saving || !enabled || fields('mode').value !== 'OUTPUT';
            const analog = fields('type').value === 'ANALOG_INPUT';
            cell.querySelector('.calibration-fields').hidden = !analog;
            fields('calibration_enabled').disabled = saving || !enabled || !analog;
            ['raw_min', 'raw_max', 'value_min', 'value_max', 'unit', 'clamp'].forEach(key => { fields(key).disabled = saving || !enabled || !analog || !fields('calibration_enabled').checked; });
            fields('initial_state').querySelector('[value="LOW"]').textContent = fields('inverted').checked ? 'ON · LOW (0)' : 'OFF · LOW (0)';
            fields('initial_state').querySelector('[value="HIGH"]').textContent = fields('inverted').checked ? 'OFF · HIGH (1)' : 'ON · HIGH (1)';
            const notes = [];
            if (INPUT_ONLY.includes(gpio)) notes.push('Entrée uniquement : pas de sortie ni de résistance pull-up/down interne.');
            if (fields('type').value === 'ANALOG_INPUT' && ADC2.includes(gpio)) notes.push('ADC2 : la lecture sera indisponible pendant le Wi-Fi. Préférez GPIO 32–39 pour l’analogique.');
            const owners = pinMetadata.get(gpio)?.usages || [];
            if (owners.length) notes.push('Usages du board : ' + owners.map(owner => owner.label).join(', ') + '.');
            if (fields('mode').value === 'OUTPUT') notes.push('L’état au démarrage s’applique au prochain boot ou à une nouvelle sortie. La polarité choisie détermine le niveau électrique de ON. Le niveau actuel reste actif ou inactif lors d’un changement de polarité.');
            cell.querySelector('.editor-help').textContent = notes.join(' ');
        };
        cell.querySelectorAll('[data-field]').forEach(input => input.addEventListener(['text', 'number'].includes(input.type) ? 'input' : 'change', () => {
            if (input.dataset.field === 'mode' && fields('mode').value === 'OUTPUT') fields('type').value = 'DIGITAL';
            local = { ...local, icon: fields('icon').value, label: fields('label').value, zone: fields('zone').value.trim(), mode: fields('mode').value, type: fields('type').value, initial_state: fields('initial_state').value, graph: fields('graph').checked, inverted: fields('inverted').checked };
            if (input.dataset.field === 'pulse_presets_ms') presetsEdited = true;
            if (local.mode === 'OUTPUT' && presetsEdited) local.pulse_presets_ms = fields('pulse_presets_ms').value.trim() ? fields('pulse_presets_ms').value.trim().split(/[;,\s]+/).map(value => Math.round(Number(value) * 1000)) : [];
            else if (local.mode !== 'OUTPUT') delete local.pulse_presets_ms;
            const number = key => fields(key).value === '' ? NaN : Number(fields(key).value);
            if (local.type === 'ANALOG_INPUT' && fields('calibration_enabled').checked) local.calibration = { raw_min: number('raw_min'), raw_max: number('raw_max'), value_min: number('value_min'), value_max: number('value_max'), unit: fields('unit').value, clamp: fields('clamp').checked };
            else delete local.calibration;
            if (fields('enabled').checked) draft.set(gpio, clone(local)); else draft.delete(gpio);
            updateFields(); updateAll(); updateHistoryOptions();
        }));
        cell.querySelectorAll('[data-capture]').forEach(button => button.addEventListener('click', () => {
            const reading = values.get(gpio), raw = reading?.value; if (!online || reading?.type !== 'ANALOG_INPUT' || typeof raw !== 'number') return;
            fields(button.dataset.capture).value = raw; fields(button.dataset.capture).dispatchEvent(new Event('input'));
        }));
        updateFields();
        updateCalibrationPreviews();
    }
    function updateHistoryOptions() {
        const select = byId('historyPin'), previous = select.value;
        const pins = [...draft.values()].filter(pin => pin.graph); select.replaceChildren();
        pins.forEach(pin => { const option = document.createElement('option'); option.value = pin.gpio; option.textContent = `${pin.label} · GPIO ${pin.gpio}`; select.append(option); });
        if ([...select.options].some(option => option.value === previous)) select.value = previous;
        byId('historyPanel').hidden = pins.length === 0; drawHistory();
    }
    function drawHistory() {
        const gpio = Number(byId('historyPin').value);
        const pin = saved.get(gpio) || draft.get(gpio), c = pin?.calibration;
        const readings = (samples.get(gpio) || []).map(sample => ({ ...sample, value: c && typeof sample.value === 'number' ? M.convert(sample.value, c) : sample.value }));
        U.chart(byId('pinChart'), readings, 'value', c ? c.unit : pin?.type === 'ANALOG_INPUT' ? 'ADC' : 'niveau');
    }
    function updateCountdowns() {
        cards.forEach((card, gpio) => {
            const pin = values.get(gpio), duration = pin?.remaining_ms || 0;
            const left = Math.max(0, duration - (Date.now() - (pin?._readAt || Date.now())));
            card.querySelector('.output-countdown').textContent = !online ? '' : duration > 0
                ? left > 0 ? `Arrêt automatique dans ${Math.ceil(left / 1000)} s` : 'Arrêt : en attente de confirmation…' : '';
        });
    }
    async function loadConfiguration(merge = false) {
        const config = await U.json('/api/io/config');
        if (!Array.isArray(config.io_pins)) throw new Error('Configuration I/O invalide.');
        const next = new Map(config.io_pins.map(pin => [pin.gpio, clone(pin)]));
        const open = merge ? [...rows].filter(([, row]) => row.querySelector('.edit-button').getAttribute('aria-expanded') === 'true').map(([gpio]) => gpio) : [];
        const focused = document.activeElement, focusedGpio = focused?.closest('.io-editor')?.id, focusedField = focused?.dataset.field;
        const selection = focusedField === 'label' ? [focused.selectionStart, focused.selectionEnd] : null;
        if (merge) {
            const nextDevice = config.device ?? null;
            if (!deviceChanged()) { draftDevice = clone(nextDevice); deviceConflict = false; }
            else if (signature(savedDocument.device ?? null) !== signature(nextDevice) && signature(draftDevice) !== signature(nextDevice)) deviceConflict = true;
            if (signature(draftDevice) === signature(nextDevice)) deviceConflict = false;
            const nextAutomation = config.automation ?? null;
            if (!automationChanged()) { draftAutomation = clone(nextAutomation); automationConflict = false; }
            else if (signature(savedDocument.automation ?? null) !== signature(nextAutomation) && signature(draftAutomation) !== signature(nextAutomation)) automationConflict = true;
            if (signature(draftAutomation) === signature(nextAutomation)) automationConflict = false;
            const nextAlerts = config.alerts ?? null;
            if (!alertsChanged()) { draftAlerts = clone(nextAlerts); alertConflict = false; }
            else if (signature(savedDocument.alerts ?? null) !== signature(nextAlerts) && signature(draftAlerts) !== signature(nextAlerts)) alertConflict = true;
            if (signature(draftAlerts) === signature(nextAlerts)) alertConflict = false;
            const nextScenes = config.scenes || [];
            if (!scenesChanged()) { draftScenes = clone(nextScenes); sceneConflict = false; }
            else if (signature(savedDocument.scenes || []) !== signature(nextScenes) && signature(draftScenes) !== signature(nextScenes)) sceneConflict = true;
            if (signature(draftScenes) === signature(nextScenes)) sceneConflict = false;
            GPIOS.forEach(gpio => {
                if (!changed(gpio)) {
                    if (next.has(gpio)) draft.set(gpio, clone(next.get(gpio))); else draft.delete(gpio);
                } else if (signature(saved.get(gpio)) !== signature(next.get(gpio)) && signature(draft.get(gpio)) !== signature(next.get(gpio))) conflicts.add(gpio);
                if (signature(draft.get(gpio)) === signature(next.get(gpio))) conflicts.delete(gpio);
            });
        } else { draftAlerts = clone(config.alerts ?? null); alertConflict = false; draftDevice = clone(config.device ?? null); deviceConflict = false; draftAutomation = clone(config.automation ?? null); automationConflict = false; draft = new Map([...next].map(([gpio, pin]) => [gpio, clone(pin)])); draftScenes = clone(config.scenes || []); conflicts.clear(); sceneConflict = false; }
        savedDocument = clone(config); saved = next; loaded = true;
        byId('sceneActions').dataset.key = ''; if (pendingImport) renderImport();
        makeRows(); open.forEach(toggleEditor); updateHistoryOptions();
        if (merge && focusedGpio && focusedField) {
            const input = document.querySelector(`#${focusedGpio} [data-field="${focusedField}"]`);
            if (input) { input.focus({ preventScroll: true }); if (selection) input.setSelectionRange(...selection); }
        }
    }
    async function refresh() {
        if (saving) return;
        const epoch = commandEpoch;
        try {
            if (!loaded) await loadConfiguration();
            const result = await U.json('/api/io/status');
            if (saving || pending.size || commandEpoch !== epoch) return;
            if (!Array.isArray(result.statuses)) throw new Error('Valeurs I/O invalides.');
            if (result._revision && result._revision !== savedDocument._revision) await loadConfiguration(true);
            if (saving || pending.size || commandEpoch !== epoch) return;
            pinMetadata = new Map((result.pins || []).map(pin => [pin.gpio, pin]));
            const next = new Map(result.statuses.map(pin => [pin.gpio, { ...pin, _readAt: Date.now() }]));
            values = next; online = true; lastSuccess = Date.now();
            values.forEach((pin, gpio) => {
                if (!samples.has(gpio)) samples.set(gpio, []);
                const history = samples.get(gpio); history.push({ t: lastSuccess, value: pin.value }); if (history.length > 120) history.shift();
            });
            U.badge('boardBadge', 'Board en ligne', 'ok'); U.text('lastUpdated', `Actualisé à ${new Date(lastSuccess).toLocaleTimeString('fr-CA')} · toutes les 2 s`);
            drawHistory();
        } catch (error) {
            if (saving || pending.size || commandEpoch !== epoch) return;
            online = false; U.badge('boardBadge', 'Board inaccessible', 'error');
            U.text('lastUpdated', lastSuccess ? `Dernière réponse à ${new Date(lastSuccess).toLocaleTimeString('fr-CA')}` : 'Connexion impossible · réessai automatique');
        }
        updateAll();
        await Promise.allSettled([refreshEvents(), refreshDiscovery()]);
    }
    async function command(gpio, value, duration = 0) {
        if (!online || saving || sceneBusy || pending.has(gpio) || changed(gpio) || !isOutput(values.get(gpio))) return;
        commandEpoch++; pending.add(gpio); updateAll();
        try {
            const result = await U.json(`/api/io/set?gpio=${gpio}&active=${value}${duration ? `&duration_ms=${duration}` : ""}`, { method: 'POST' });
            if (result.gpio !== gpio || Number(result.active) !== value) throw new Error('Le board n’a pas confirmé le niveau demandé.');
            values.set(gpio, { ...values.get(gpio), ...result, _readAt: Date.now() });
            U.notice(`${saved.get(gpio)?.label || `GPIO ${gpio}`} : ${value ? 'ON' : 'OFF'} · ${result.value ? 'HIGH (1)' : 'LOW (0)'} confirmé${duration ? ` pour ${duration / 1000} s` : ''} par le board.`);
            await refreshEvents();
        } catch (error) {
            addFailure(gpio);
            online = false; U.badge('boardBadge', 'Commande non confirmée', 'error');
            U.notice(`GPIO ${gpio} : ${error.message}`, true);
        } finally { pending.delete(gpio); updateAll(); }
    }
    async function saveConfiguration() {
        if (!online || saving || !changeCount() || pending.size || hasConflicts()) return;
        const config = payload();
        const error = M.validate(config); if (error) { U.notice(error, true); return; }
        commandEpoch++; saving = true; updateAll(); editors.forEach(editor => editor.querySelectorAll('input, select').forEach(input => { input.disabled = true; }));
        let accepted = false;
        try {
            const result = await U.json('/api/io/config', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(config) });
            if (result.status !== 'success') throw new Error(result.message || 'Enregistrement non confirmé.');
            accepted = true;
            await loadConfiguration(); U.notice('Configuration enregistrée et appliquée.');
        } catch (error) {
            U.notice(accepted ? `Configuration appliquée, mais la relecture a échoué : ${error.message}. Actualisez avant de continuer.` : `Enregistrement non confirmé : ${error.message}. Vos modifications sont conservées.`, true);
            if (accepted) { loaded = false; online = false; }
        } finally {
            saving = false;
            // Reopen editors to restore enabled controls after a rejected save.
            if (!accepted) rows.forEach((row, gpio) => {
                if (row.querySelector('.edit-button').getAttribute('aria-expanded') === 'true') { toggleEditor(gpio); toggleEditor(gpio); }
            });
            updateAll(); await refresh();
        }
    }
    function selectFilter(nextFilter) {
        filter = nextFilter;
        document.querySelectorAll('[data-filter]').forEach(button => { const active = button.dataset.filter === filter; button.classList.toggle('active', active); button.setAttribute('aria-pressed', String(active)); });
        rememberPreferences(); applyFilters();
    }
    if (typeof preferences.search === 'string') byId('pinSearch').value = preferences.search.slice(0, 100);
    byId('pinSearch').addEventListener('input', () => { rememberPreferences(); applyFilters(); });
    document.querySelectorAll('[data-filter]').forEach(button => button.addEventListener('click', () => selectFilter(button.dataset.filter)));
    byId('showOutputs').addEventListener('click', () => { byId('pinSearch').value = ''; selectFilter('output'); byId('pinRows').closest('section').scrollIntoView({ behavior: 'smooth', block: 'start' }); });
    byId('allOff').addEventListener('click', () => runScene('all_off'));
    byId('newScene').addEventListener('click', () => {
        if (!loaded || saving || pending.size || draftScenes.length >= 12) return;
        editingScene = `scene_${Date.now().toString(36)}_${Math.random().toString(36).slice(2, 7)}`;
        draftScenes.push({ id: editingScene, name: 'Nouvelle scène', actions: [] }); updateAll();
        byId('sceneEditor').scrollIntoView({ behavior: 'smooth', block: 'center' }); byId('sceneName').focus(); byId('sceneName').select();
    });
    byId('sceneName').addEventListener('input', () => {
        const scene = draftScenes.find(scene => scene.id === editingScene); if (!scene) return;
        scene.name = byId('sceneName').value; byId('sceneActions').dataset.key = sceneEditorKey(scene); updateAll();
    });
    byId('sceneIcon').replaceChildren(...IOModel.iconOptions());
    byId('sceneIcon').addEventListener('change', () => { const scene = draftScenes.find(scene => scene.id === editingScene); if (scene) { scene.icon = byId('sceneIcon').value; updateAll(); } });
    byId('timedScene').addEventListener('click', () => {
        if (!loaded || saving || pending.size || draftScenes.length >= 12) return;
        const pin = [...draft.values()].find(pin => isOutput(pin) && typeof values.get(pin.gpio)?.value === 'number');
        if (!pin) return U.notice('Configurez une sortie disponible pour créer une activation temporisée.', true);
        const scene = { id: IOModel.newId('scene'), name: 'Activation temporisée', icon: pin.icon || 'scene', zone: pin.zone || '', actions: [{ gpio: pin.gpio, active: true, duration_ms: 1000 }] };
        draftScenes.push(scene); editingScene = scene.id; updateAll(); byId('sceneEditor').scrollIntoView({ behavior: 'smooth' }); U.notice('Modèle dans le brouillon : ON une seconde, sans commande exécutée.');
    });
    byId('sceneZone').addEventListener('input', () => { const scene = draftScenes.find(scene => scene.id === editingScene); if (scene) { scene.zone = byId('sceneZone').value.trim(); byId('sceneActions').dataset.key = sceneEditorKey(scene); updateAll(); } });
    byId('zoneFilter').addEventListener('change', applyFilters);
    byId('closeScene').addEventListener('click', () => { editingScene = ''; updateScenes(); });
    byId('deleteScene').addEventListener('click', () => { draftScenes = draftScenes.filter(scene => scene.id !== editingScene); editingScene = ''; updateAll(); });
    byId('importButton').addEventListener('click', () => byId('importFile').click());
    byId('importFile').addEventListener('change', () => readImport(byId('importFile').files[0]));
    byId('cancelImport').addEventListener('click', () => { pendingImport = null; byId('importPanel').hidden = true; });
    byId('stageImport').addEventListener('click', () => {
        if (!pendingImport || saving || pending.size) return;
        draftAlerts = clone(pendingImport.alerts); alertConflict = false; draftDevice = clone(pendingImport.device); deviceConflict = false; draftAutomation = clone(pendingImport.automation); automationConflict = false;
        draft = new Map(pendingImport.io_pins.map(pin => [pin.gpio, clone(pin)])); draftScenes = clone(pendingImport.scenes);
        conflicts.clear(); sceneConflict = false; editingScene = ''; pendingImport = null; byId('importPanel').hidden = true;
        makeRows(); updateAll(); updateHistoryOptions(); U.notice('Import chargé dans le brouillon. Vérifiez les modifications, puis enregistrez pour les appliquer.');
        byId('saveBar').scrollIntoView({ behavior: 'smooth', block: 'center' });
    });
    byId('clearActivity').addEventListener('click', () => { hiddenThrough = eventCursor; for (let i = activity.length - 1; i >= 0; i--) if (activity[i].id == null) activity.splice(i, 1); renderActivity(); });
    byId('activitySource').addEventListener('change', renderActivity);
    byId('exportActivity').addEventListener('click', () => U.download('iyak32-io-journal.json', JSON.stringify({ session: eventSession, events: visibleEvents() }, null, 2), 'application/json'));
    byId('saveButton').addEventListener('click', saveConfiguration);
    byId('discardButton').addEventListener('click', () => {
        draftAlerts = clone(savedDocument.alerts ?? null); alertConflict = false; draftDevice = clone(savedDocument.device ?? null); deviceConflict = false; draftAutomation = clone(savedDocument.automation ?? null); automationConflict = false; conflicts.clear(); sceneConflict = false; editingScene = ''; draftScenes = clone(savedDocument.scenes || []); draft = new Map([...saved].map(([gpio, pin]) => [gpio, clone(pin)])); makeRows(); updateAll(); updateHistoryOptions(); U.notice('Modifications annulées.');
    });
    byId('exportButton').addEventListener('click', () => {
        U.download(changeCount() ? 'iyak32-io-modifications.json' : 'iyak32-io-config.json', JSON.stringify(payload(), null, 2), 'application/json');
    });
    byId('historyPin').addEventListener('change', drawHistory);
    window.addEventListener('beforeunload', event => { if (changeCount()) { event.preventDefault(); event.returnValue = ''; } });
    document.addEventListener('keydown', event => { if ((event.ctrlKey || event.metaKey) && event.key.toLowerCase() === 's') { event.preventDefault(); saveConfiguration(); } });
    byId('keepDrafts').addEventListener('click', () => { conflicts.clear(); sceneConflict = false; automationConflict = false; deviceConflict = false; alertConflict = false; updateAll(); U.notice('Vos choix sont conservés. Enregistrer remplacera les valeurs affichées dans le comparatif.'); });
    byId('reloadDrafts').addEventListener('click', () => byId('discardButton').click());
    setInterval(updateCountdowns, 250);
    const requestedPin = Number(new URLSearchParams(location.search).get('gpio'));
    selectFilter(filter);
    if (new URLSearchParams(location.search).has('gpio') && GPIOS.includes(requestedPin)) { selectFilter('all'); byId('pinSearch').value = `gpio ${requestedPin}`; }
    const poll = U.poll(refresh, 2000);
    byId('refreshButton').addEventListener('click', poll);
})();
