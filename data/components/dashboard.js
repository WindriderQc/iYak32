(() => {
    const ui = BoardUI;
    const active = pin => typeof pin?.active === "boolean" ? pin.active : !!pin?.value !== !!pin?.inverted;
    let pins = null, lastSuccess = 0, pendingPin = null;
    let configuration = null, readings = null, board = null, selection = null, storageKey = '', actionBusy = false, automation = null;
    let thresholdStatus = null, activityEvents = [], activitySession = '', activityCursor = 0, activityOnline = false;
    const M = IOModel, $ = id => document.getElementById(id);
    const overrideDurations = new Map();
    const selected = () => selection ?? (pins || []).filter(M.output).map(pin => `pin:${pin.gpio}`);
    const title = ref => ref.startsWith('pin:') ? pins?.find(pin => pin.gpio === Number(ref.slice(4)))?.label || `GPIO ${ref.slice(4)} retiré` : ref.startsWith('scene:') ? configuration?.scenes?.find(scene => scene.id === ref.slice(6))?.name || 'Scène retirée' : ({ temperature: 'Température', pressure: 'Pression', humidity: 'Humidité', battery: 'Batterie' })[ref.slice(7)];
    function remember() { try { localStorage.setItem(storageKey, JSON.stringify(selection)); } catch (_) { ui.notice('Raccourcis actifs pour cette page. Stockage du navigateur indisponible.'); } }
    function personalize() {
        const choice = $('favoriteChoice'), before = choice.value;
        choice.replaceChildren(...[...(pins || []).map(pin => [`pin:${pin.gpio}`, `${pin.label} · GPIO ${pin.gpio}${pin.zone ? ' · ' + pin.zone : ''}`]), ...(configuration?.scenes || []).map(scene => [`scene:${scene.id}`, `Scène · ${scene.name}`]), ...['temperature', 'pressure', 'humidity', 'battery'].map(key => [`metric:${key}`, title(`metric:${key}`)])].filter(([ref]) => !selected().includes(ref)).map(([ref, label]) => new Option(label, ref)));
        if ([...choice.options].some(option => option.value === before)) choice.value = before;
        $('addFavorite').disabled = !choice.options.length; $('favoriteList').replaceChildren();
        selected().forEach((ref, index) => {
            const li = document.createElement('li'), name = document.createElement('span'), actions = document.createElement('div'); name.textContent = title(ref); actions.className = 'toolbar';
            [['↑', -1], ['↓', 1], ['Retirer', 0]].forEach(([label, delta]) => { const b = document.createElement('button'); b.className = 'btn secondary'; b.textContent = label; b.setAttribute('aria-label', `${label} ${title(ref)}`); b.disabled = delta < 0 && index === 0 || delta > 0 && index === selected().length - 1; b.addEventListener('click', () => { selection = [...selected()]; if (!delta) selection.splice(index, 1); else [selection[index], selection[index + delta]] = [selection[index + delta], selection[index]]; remember(); personalize(); outputs(readings); }); actions.append(b); });
            li.append(name, actions); $('favoriteList').append(li);
        });
    }
    async function command(pin, on, duration = 0) {
        if (actionBusy) return;
        actionBusy = true; pendingPin = pin.gpio; outputs(readings); ui.notice('Commande en cours…');
        try { await ui.json(`/api/io/set?gpio=${pin.gpio}&active=${on ? 1 : 0}${duration ? `&duration_ms=${duration}` : ''}`, { method: 'POST' }); const next = await ui.json('/api/io/status'); readings = next.statuses; const applied = readings.find(state => state.gpio === pin.gpio); if (!applied || applied.value == null) throw new Error('État indisponible'); ui.notice(`${pin.label} : ${active(applied) ? 'allumée' : 'éteinte'}.`); }
        catch (e) { readings = null; ui.notice(`Commande non confirmée : ${e.message}`, true); }
        finally { pendingPin = null; actionBusy = false; outputs(readings); }
    }
    async function sceneCommand(id) {
        if (actionBusy || !readings || !configuration?._revision) return;
        actionBusy = true; outputs(readings); ui.notice('Scène en cours…');
        try { const result = await ui.json('/api/io/scene', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ id, _revision: configuration._revision }) }); ui.notice(`${result.name} exécutée.`); readings = (await ui.json('/api/io/status')).statuses; }
        catch (e) { readings = null; ui.notice(`Scène non confirmée : ${e.message}`, true); }
        finally { actionBusy = false; outputs(readings); }
    }
    function outputs(statuses) {
        zones(statuses);
        const container = $('quickControls'); container.replaceChildren();
        ($('dashboardZone').value ? [...(pins || []).filter(pin => pin.zone === $('dashboardZone').value).map(pin => `pin:${pin.gpio}`), ...(configuration?.scenes || []).filter(scene => scene.zone === $('dashboardZone').value).map(scene => `scene:${scene.id}`)] : selected()).forEach(ref => {
            const pin = pins?.find(pin => ref === `pin:${pin.gpio}`), scene = configuration?.scenes?.find(scene => ref === `scene:${scene.id}`), zone = pin?.zone || scene?.zone || '';
            if ($('dashboardZone').value && zone !== $('dashboardZone').value) return;
            const card = document.createElement('div'); card.className = 'favorite-card'; card.dataset.ref = ref;
            const reading = statuses?.find(state => state.gpio === pin?.gpio), known = reading && typeof reading.value === 'number';
            if (pin && M.output(pin)) {
                const button = document.createElement('button'); button.type = 'button'; button.className = 'output-button'; button.dataset.gpio = pin.gpio; button.dataset.on = known ? String(active(reading)) : 'unknown'; button.disabled = !known || actionBusy;
                if (known) button.setAttribute('aria-pressed', String(active(reading)));
                const name = document.createElement('strong'); name.textContent = `${M.icon(pin)} ${pin.label}`; const gpio = document.createElement('small'); gpio.textContent = `GPIO ${pin.gpio}${zone ? ' · ' + zone : ''}`; const value = document.createElement('span'); value.className = 'output-state'; value.textContent = pendingPin === pin.gpio ? 'En cours…' : !known ? reading?.reason || 'État inconnu' : active(reading) ? 'Allumée' : 'Éteinte';
                button.append(name, gpio, value); button.addEventListener('click', () => command(pin, !active(reading))); card.append(button);
                const manual = document.createElement('div'); manual.className = 'toolbar override-controls';
                const length = document.createElement('select'); length.setAttribute('aria-label', `Durée de priorité ${pin.label}`);
                [[60000, '1 min'], [900000, '15 min'], [3600000, '1 h']].forEach(([ms, name]) => length.add(new Option(name, ms))); length.value = overrideDurations.get(pin.gpio) || 900000;
                length.disabled = !known || actionBusy || !!reading?.override_remaining_ms;
                length.addEventListener('change', () => overrideDurations.set(pin.gpio, Number(length.value)));
                const hold = document.createElement('button'); hold.className = 'btn secondary'; hold.textContent = reading?.override_remaining_ms ? `Reprendre · ${Math.ceil(reading.override_remaining_ms / 1000)} s` : 'Prendre la main'; hold.disabled = !known || actionBusy;
                hold.addEventListener('click', () => override(pin, reading?.override_remaining_ms ? 0 : Number(length.value))); manual.append(length, hold); card.append(manual);
                if (known && reading.remaining_ms) { const countdown = document.createElement('small'); countdown.textContent = `Arrêt dans ${Math.ceil(reading.remaining_ms / 1000)} s`; card.append(countdown); }
                const presets = document.createElement('div'); presets.className = 'toolbar';
                (pin.pulse_presets_ms ?? [5000, 30000, 60000]).forEach(ms => { const b = document.createElement('button'); b.className = 'btn secondary'; b.textContent = ms % 60000 === 0 ? `${ms / 60000} min` : `${ms / 1000} s`; b.disabled = !known || actionBusy; b.addEventListener('click', () => command(pin, true, ms)); presets.append(b); }); card.append(presets);
            } else {
                const name = document.createElement('strong'); name.textContent = `${M.icon(pin || scene)} ${title(ref)}`; const detail = document.createElement('small'); detail.textContent = `${pin ? `GPIO ${pin.gpio}` : scene ? 'Scène' : 'Mesure ambiante'}${zone ? ' · ' + zone : ''}`; card.append(name, detail);
                if (scene) { const run = document.createElement('button'); run.className = 'btn secondary'; run.textContent = 'Lancer'; run.disabled = actionBusy || !configuration?._revision || !statuses || scene.actions.some(action => typeof statuses.find(state => state.gpio === action.gpio)?.value !== 'number'); run.addEventListener('click', () => sceneCommand(scene.id)); card.append(run); }
                else { const value = document.createElement('span'); value.className = 'metric-number'; const metrics = { temperature: ['bmx_temp', '°C'], pressure: ['bmx_pressure', 'hPa'], humidity: ['bmx_humidity', '%'], battery: ['battery', 'V'] }, metric = metrics[ref.slice(7)]; const measured = pin ? known ? pin.type === 'ANALOG_INPUT' ? reading.calibrated_value ?? reading.value : reading.value : null : board && metric ? board[metric[0]] : null;
                    value.textContent = measured == null ? 'Indisponible' : pin?.type === 'DIGITAL' ? measured ? 'HIGH' : 'LOW' : `${ui.number(measured)} ${pin ? reading.unit || 'ADC' : metric?.[1] || ''}`; card.append(value);
                    if (pin && known && reading.calibrated_value != null) { const raw = document.createElement('small'); raw.textContent = `ADC brut : ${reading.value}`; card.append(raw); }
                }
            }
            if (pin) { const link = document.createElement('a'); link.className = 'io-detail-link'; link.href = `/io_detail.html?gpio=${pin.gpio}`; link.textContent = 'Ouvrir la fiche →'; card.append(link); }
            container.append(card);
        });
        if (!container.children.length) { const p = document.createElement('p'); p.className = 'empty-state'; p.textContent = pins === null ? 'Configuration indisponible.' : 'Aucun raccourci dans cette vue. Personnalisez votre tableau de bord.'; container.append(p); }
        $('dashboardStop').disabled = !statuses || actionBusy;
    }
    let nameDirty = false, nameRevision = '';
    $('boardIcon').replaceChildren(...M.iconOptions());
    for (const id of ['boardFriendlyName', 'boardIcon']) $(id).addEventListener('input', () => { if (!nameDirty) nameRevision = configuration?._revision; nameDirty = true; });
    $('reloadBoardName').addEventListener('click', () => { nameDirty = false; nameRevision = ''; $('boardFriendlyName').value = configuration?.device?.name || board?.friendly_name || board?.device_name || ''; $('boardIcon').value = configuration?.device?.icon || ''; });
    $('boardNameForm').addEventListener('submit', async event => {
        event.preventDefault(); if (!configuration || actionBusy) return;
        const next = { ...configuration, _revision: nameDirty ? nameRevision : configuration._revision, device: { name: $('boardFriendlyName').value.trim(), icon: $('boardIcon').value } };
        const error = M.validate(next); if (error) return ui.notice(error, true);
        actionBusy = true; $('saveBoardName').disabled = true;
        try { await ui.json('/api/io/config', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(next) }); nameDirty = false; ui.notice('Nom et icône enregistrés.'); await refresh(); }
        catch (e) { ui.notice(`Nom non enregistré : ${e.message}`, true); }
        finally { actionBusy = false; $('saveBoardName').disabled = false; }
    });
    async function override(pin, duration) {
        if (actionBusy) return; actionBusy = true; outputs(readings);
        try { await ui.json(`/api/io/override?gpio=${pin.gpio}&duration_ms=${duration}`, { method: 'POST' }); readings = (await ui.json('/api/io/status')).statuses; ui.notice(duration ? `${pin.label} : priorité manuelle activée.` : `${pin.label} : automatismes autorisés, sans rattrapage.`); }
        catch (e) { readings = null; ui.notice(e.message, true); }
        finally { actionBusy = false; outputs(readings); }
    }
    async function stopZone(zone) {
        if (actionBusy || !configuration?._revision) return; actionBusy = true; outputs(readings);
        try { const result = await ui.json('/api/io/zone/off', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ zone, hold_ms: 900000, _revision: configuration._revision }) }); readings = (await ui.json('/api/io/status')).statuses; ui.notice(`${zone} : sorties OFF, priorité manuelle 15 min.${result.skipped?.length ? ` GPIO ignorés : ${result.skipped.join(', ')}.` : ''}`); }
        catch (e) { readings = null; ui.notice(e.message, true); }
        finally { actionBusy = false; outputs(readings); }
    }
    async function resumeZone(zone) {
        if (actionBusy || !configuration?._revision) return; actionBusy = true; outputs(readings);
        try {
            const result = await ui.json('/api/io/zone/resume', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ zone, _revision: configuration._revision }) });
            readings = (await ui.json('/api/io/status')).statuses;
            ui.notice(`${zone} : ${result.outputs.length} priorité(s) libérée(s), sorties et minuteries conservées.${automation?.paused ? ' La pause globale reste active.' : ' Sans rattrapage des actions manquées.'}${result.skipped?.length ? ` GPIO ignorés : ${result.skipped.join(', ')}.` : ''}`);
        } catch (e) { readings = null; ui.notice(`Reprise non confirmée : ${e.message}`, true); }
        finally { actionBusy = false; outputs(readings); }
    }
    function zones(statuses) {
        const list = $('zoneCards'); list.replaceChildren();
        const names = [...new Set([...(pins || []), ...(configuration?.scenes || [])].map(item => item.zone).filter(Boolean))].sort();
        if (!names.length) { const p = document.createElement('p'); p.className = 'empty-state'; p.textContent = 'Attribuez une zone à vos I/O : Atelier, Garage ou Bac 1.'; list.append(p); return; }
        names.forEach(zone => {
            const card = document.createElement('article'); card.className = 'control-card'; card.dataset.zone = zone;
            const title = document.createElement('h3'); title.textContent = zone;
            const mode = M.zoneState(configuration, statuses, automation, zone), badge = document.createElement('span'); badge.className = 'status-pill zone-mode'; badge.dataset.mode = mode.mode;
            badge.textContent = ({ unknown: 'État inconnu', readings: 'Mesures', unavailable: 'Sorties indisponibles', manual: 'Manuel', mixed: 'Mixte', paused: 'Automatismes en pause', auto: 'Auto autorisé' })[mode.mode]; badge.dataset.state = mode.mode === 'auto' || mode.mode === 'readings' ? 'ok' : 'warning';
            const heading = document.createElement('div'); heading.className = 'panel-head'; heading.append(title, badge);
            const members = (pins || []).filter(pin => pin.zone === zone), available = members.filter(M.output).filter(pin => typeof statuses?.find(r => r.gpio === pin.gpio)?.value === 'number');
            const activeCount = available.filter(pin => active(statuses.find(r => r.gpio === pin.gpio))).length;
            const note = document.createElement('p'); note.className = 'control-status'; note.textContent = statuses ? `${activeCount} sortie(s) active(s) / ${available.length} disponible(s)` : 'État indisponible';
            const priority = document.createElement('p'); priority.className = 'zone-priority control-status'; priority.textContent = mode.held ? `${mode.held}/${mode.available} sortie(s) disponible(s) en priorité manuelle · dernière reprise dans ${Math.ceil(mode.remaining / 1000)} s.${mode.paused ? ' Pause globale toujours active.' : ''}` : mode.mode === 'auto' ? 'Aucune priorité manuelle. Les automatismes peuvent commander cette zone.' : mode.paused ? 'Reprendre une priorité manuelle ne lève pas la pause globale.' : '';
            const detail = document.createElement('ul'); detail.className = 'zone-readings';
            members.forEach(pin => { const r = statuses?.find(r => r.gpio === pin.gpio), li = document.createElement('li'); li.textContent = `${M.icon(pin)} ${pin.label} : ${typeof r?.value !== 'number' ? 'indisponible' : M.output(pin) ? `${active(r) ? 'ON' : 'OFF'}${r.remaining_ms ? ` · arrêt ${Math.ceil(r.remaining_ms / 1000)} s` : ''}${r.override_remaining_ms ? ` · manuel ${Math.ceil(r.override_remaining_ms / 1000)} s` : ''}` : pin.type === 'ANALOG_INPUT' ? `${ui.number(r.calibrated_value ?? r.value)} ${r.unit || 'ADC'}` : r.value ? 'HIGH' : 'LOW'}`; detail.append(li); });
            const related = item => item.scene_id === 'all_off' || configuration?.scenes?.find(scene => scene.id === item.scene_id)?.actions.some(action => members.some(pin => pin.gpio === action.gpio));
            const scheduleIDs = (configuration?.automation?.schedules || []).filter(related).map(item => item.id), next = automation?.items.filter(item => scheduleIDs.includes(item.id) && item.next_epoch).sort((a, b) => a.next_epoch - b.next_epoch)[0];
            const last = IOInsights.lastActivation(activityEvents, configuration, zone), recent = document.createElement('p'); recent.className = 'zone-last-action panel-note';
            recent.textContent = last ? `Dernière activation reçue : ${last.label} · ${IOInsights.SOURCES[last.source] || last.source} · ${new Date(last.at).toLocaleTimeString('fr-CA')}${!activityOnline ? ' · journal inaccessible' : ''}` : activityOnline ? 'Aucune activation dans le journal actuel.' : 'Dernière activation indisponible.';
            if (last) { const link = document.createElement('a'); link.className = 'zone-activity-link'; link.href = last.trace ? `/activity.html?trace=${last.trace}&session=${encodeURIComponent(activitySession)}` : `/activity.html?gpio=${last.gpio}`; link.textContent = ' Voir la séquence →'; recent.append(link); const history = document.createElement('a'); history.href = `/io_detail.html?gpio=${last.gpio}`; history.textContent = ' Courbe →'; recent.append(history); }
            const upcoming = document.createElement('p'); upcoming.className = 'panel-note'; upcoming.textContent = !automation ? 'Automatismes indisponibles' : automation.paused ? 'Automatismes en pause' : next ? 'Prochaine action : ' + new Date(next.next_epoch * 1000).toLocaleString('fr-CA', { timeZone: automation.timezone }) : 'Aucun horaire à venir';
            const actions = document.createElement('div'); actions.className = 'toolbar';
            const view = document.createElement('button'); view.className = 'btn secondary'; view.textContent = 'Voir les commandes'; view.addEventListener('click', () => { $('dashboardZone').value = zone; outputs(readings); $('quickControls').scrollIntoView({ behavior: 'smooth' }); });
            const stop = document.createElement('button'); stop.className = 'btn secondary'; stop.textContent = 'Tout OFF · priorité 15 min'; stop.disabled = actionBusy || !available.length; stop.addEventListener('click', () => stopZone(zone));
            const resume = document.createElement('button'); resume.className = 'btn secondary zone-resume'; resume.textContent = 'Reprendre la zone'; resume.disabled = actionBusy || !mode.held; resume.addEventListener('click', () => resumeZone(zone));
            actions.append(view, stop, resume); card.append(heading, note, priority, detail, recent, upcoming, actions); list.append(card);
        });
    }
    $('personalizeButton').addEventListener('click', () => { $('personalizePanel').hidden = !$('personalizePanel').hidden; personalize(); });
    $('closePersonalize').addEventListener('click', () => { $('personalizePanel').hidden = true; });
    $('addFavorite').addEventListener('click', () => { if (!$('favoriteChoice').value) return; selection = [...selected(), $('favoriteChoice').value]; remember(); personalize(); outputs(readings); });
    $('resetFavorites').addEventListener('click', () => { selection = null; remember(); personalize(); outputs(readings); });
    $('dashboardZone').addEventListener('change', () => outputs(readings));
    $('dashboardStop').addEventListener('click', async () => {
        if (actionBusy || !readings) return;
        actionBusy = true; outputs(readings); let pauseError = '';
        try {
            let latest = await ui.json('/api/io/config'); const settings = { ...(latest.automation || M.defaultAutomation()), paused: true };
            try { await ui.json('/api/io/config', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ ...latest, automation: settings }) }); } catch (e) { pauseError = e.message; }
            latest = await ui.json('/api/io/config'); const stopped = await ui.json('/api/io/scene', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ id: 'all_off', _revision: latest._revision }) });
            ui.notice(pauseError ? `Sorties arrêtées. Pause non confirmée : ${pauseError}` : `Sorties arrêtées et automatismes en pause.${stopped.skipped?.length ? ` GPIO indisponibles : ${stopped.skipped.join(', ')}.` : ''}`, !!pauseError);
        } catch (e) { ui.notice(`Arrêt non confirmé : ${e.message}`, true); }
        finally { actionBusy = false; await refresh(); }
    });
    let alertSignature = '';
    function renderAlerts(io) {
        const items = IOInsights.alerts(board, configuration, io, automation, thresholdStatus), signature = JSON.stringify(items);
        if (signature === alertSignature) return; alertSignature = signature;
        $('dashboardAlerts').replaceChildren();
        items.forEach(item => { const li = document.createElement('li'), a = document.createElement('a'); li.dataset.alert = item.key; a.href = item.href; a.textContent = item.text; li.append(a); $('dashboardAlerts').append(li); });
        $('alertsEmpty').hidden = !!items.length; ui.badge('alertsBadge', items.length ? `${items.length} à vérifier` : 'Tout va bien', items.length ? 'warning' : 'ok');
    }
    async function refresh() {
        const [status, mqtt, io, history, config, auto, thresholds, activity] = await Promise.allSettled([
            ui.json('/api/status'), ui.json('/api/mqtt/status'), ui.json('/api/io/status'),
            ui.json('/api/logs/sensors'), ui.json('/api/io/config'), ui.json('/api/io/automation/status'),
            ui.json('/api/io/alerts/status'), ui.json(`/api/io/activity?after=${activityCursor}&session=${encodeURIComponent(activitySession)}`)
        ]);
        thresholdStatus = thresholds.status === 'fulfilled' ? thresholds.value : null; activityOnline = activity.status === 'fulfilled' && Array.isArray(activity.value.events) && typeof activity.value.session === 'string';
        if (activityOnline) { const h = activity.value; if (h.reset || h.lost || activitySession && activitySession !== h.session) { activityEvents = []; activityCursor = 0; } activitySession = h.session; h.events.filter(e => e.id > activityCursor).forEach(e => activityEvents.push({ ...e, at: Date.now() - ((h.now_ms - e.at_ms) >>> 0) })); activityEvents = activityEvents.slice(-64); activityCursor = h.latest_id; }
        if (config.status === 'fulfilled' && config.value) { configuration = config.value; pins = config.value.io_pins || []; }
        if (status.status !== 'fulfilled') {
            ui.badge('boardBadge', 'Board inaccessible', 'error'); ui.badge('wifiBadge', 'Wi-Fi inconnu'); ui.badge('mqttBadge', 'MQTT inconnu');
            ui.badge('sampleBadge', 'Mesures périmées', 'warning');
            ui.text('lastUpdated', lastSuccess ? `Dernière réponse ${ui.age(Date.now() - lastSuccess)}` : 'Aucune réponse du board');
            board = null; readings = null; if (!actionBusy) outputs(null); $('automationSummary').textContent = 'État des automatismes indisponible.'; automation = null; renderAlerts(null); return;
        }
        const d = status.value; board = d; lastSuccess = Date.now();
        const nextKey = 'iyak32.dashboard.' + d.device_name; if (storageKey !== nextKey) { storageKey = nextKey; try { const stored = JSON.parse(localStorage.getItem(storageKey) || 'null'); selection = Array.isArray(stored) ? [...new Set(stored.filter(ref => typeof ref === 'string' && /^(pin|scene|metric):/.test(ref)))].slice(0, 42) : null; } catch (_) { selection = null; } }
        const zone = $('dashboardZone').value, zones = [...new Set([...(pins || []), ...(configuration?.scenes || [])].map(item => item.zone).filter(Boolean))].sort(); $('dashboardZone').replaceChildren(new Option('Toutes les zones', ''), ...zones.map(zone => new Option(zone, zone))); if (zones.includes(zone)) $('dashboardZone').value = zone; personalize();
        automation = auto.status === 'fulfilled' ? auto.value : null;
        const next = automation?.items.filter(item => item.next_epoch).sort((a, b) => a.next_epoch - b.next_epoch)[0];
        $('automationSummary').textContent = !automation ? 'État des automatismes indisponible.' : automation.paused ? 'Automatismes en pause.' : next ? 'Prochain horaire : ' + new Date(next.next_epoch * 1000).toLocaleString('fr-CA', { timeZone: automation.timezone }) : 'Automatismes actifs · aucun horaire à venir.';
        HALinks.link($('dashboardHADevice'), HALinks.device(configuration)); HALinks.link($('dashboardHA24'), HALinks.history(configuration, '', 24)); HALinks.link($('dashboardHA7'), HALinks.history(configuration, '', 168));
        $('dashboardHANote').textContent = HALinks.available(configuration) ? 'Historique et contrôles ouverts dans votre session Home Assistant.' : 'Configurez les liens dans la page Historique.';
        ui.badge('boardBadge', 'Board en ligne', 'ok');
        ui.badge('wifiBadge', d.wifi_connected ? 'Wi-Fi connecté' : 'Wi-Fi déconnecté', d.wifi_connected ? 'ok' : 'warning');
        ui.badge('mqttBadge', d.mqtt_enabled ? d.mqtt_state === 'connected' ? 'MQTT connecté' : 'MQTT en reconnexion' : 'MQTT désactivé', d.mqtt_state === 'connected' ? 'ok' : 'warning');
        const stale = d.sensor_age_ms == null || d.sensor_age_ms > Math.max(15000, (d.sample_interval_sec || 5) * 3000);
        ui.badge('sampleBadge', stale ? 'Mesure indisponible' : ui.age(d.sensor_age_ms), stale ? 'warning' : 'ok');
        ui.text('lastUpdated', 'Actualisé à ' + new Date().toLocaleTimeString('fr-CA'));
        ui.text('dashboardTitle', `${M.icon(configuration?.device || { icon: 'home' })} ${configuration?.device?.name || d.friendly_name || 'Tableau de bord'}`);
        ui.text('deviceSubtitle', `${configuration?.device?.name || d.friendly_name || d.device_name} · ${d.wifi_ip} · ${d.mode === 'basic' ? 'Basic Mode' : d.mode || 'Capteurs'}`);
        ui.text('temperature', ui.number(d.bmx_temp)); ui.text('pressure', ui.number(d.bmx_pressure) + ' hPa');
        ui.text('climateDescription', d.bmx_temp == null ? 'Capteur ambiant indisponible' : `Une mesure toutes les ${d.sample_interval_sec || 5} secondes`);
        document.getElementById('humidityMetric').hidden = d.bmx_humidity == null;
        ui.text('humidity', ui.number(d.bmx_humidity) + ' %');
        ui.text('battery', ui.number(d.battery, 2)); ui.text('cpuTemperature', ui.number(d.cpu_temp)); ui.text('freeHeap', ui.number(d.free_heap / 1024, 0));
        ui.text('wifiName', d.wifi_ssid || 'Aucun réseau'); ui.text('wifiAddress', d.wifi_ip); ui.text('wifiSignal', `${d.wifi_rssi ?? '—'} dBm`);
        ui.text('deviceVersion', d.version); ui.text('deviceUptime', `En service depuis ${ui.uptime(d.uptime)}`);
        if (mqtt.status === 'fulfilled') {
            const m = mqtt.value; ui.text('brokerAddress', m.server ? `${m.server}:${m.port}` : 'Non configuré');
            ui.text('mqttDescription', m.connected ? 'Échange de données actif' : m.enabled ? 'Tentative de reconnexion' : 'Désactivé dans les réglages');
        } else { ui.text('brokerAddress', 'État indisponible'); ui.text('mqttDescription', 'Réessayez dans quelques secondes'); }
        if (document.activeElement !== $('boardFriendlyName') && !nameDirty) { $('boardFriendlyName').value = configuration?.device?.name || d.friendly_name || d.device_name; $('boardIcon').value = configuration?.device?.icon || ''; }
        if (!actionBusy) { readings = io.status === 'fulfilled' ? io.value.statuses : null; outputs(readings); }
        renderAlerts(io.status === 'fulfilled' ? io.value : null);
        if (history.status === 'fulfilled') {
            const rows = history.value; ui.chart(document.getElementById('temperatureChart'), rows, 'temp', '°C');
            ui.text('historyNote', rows.length > 1 ? `${rows.length} mesures · ${ui.number((rows.at(-1).t - rows[0].t) / 60000)} min sur le board` : 'L’historique se remplit à chaque mesure.');
        } else ui.text('historyNote', 'Historique momentanément indisponible.');
    }
    const run = ui.poll(refresh); document.getElementById('refreshButton').addEventListener('click', run);
})();
