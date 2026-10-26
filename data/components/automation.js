(() => {
    const U = BoardUI, M = IOModel, $ = id => document.getElementById(id), clone = value => JSON.parse(JSON.stringify(value));
    const days = ['Dim', 'Lun', 'Mar', 'Mer', 'Jeu', 'Ven', 'Sam'];
    const stateNames = { paused: 'En pause', disabled: 'Désactivé', clock_unavailable: 'Heure indisponible', outside_window: 'Hors plage horaire', input_unavailable: 'Entrée indisponible', initializing: 'Lecture initiale', condition_active: 'Condition active · attente du retour', armed: 'Prêt' };
    let config = null, draft = M.defaultAutomation(), runtime = null, online = false, busy = false, conflict = false, editing = null;
    const dirty = () => M.signature(draft) !== M.signature(config?.automation || M.defaultAutomation());
    const candidate = () => ({ ...config, automation: clone(draft) });
    const sceneName = id => id === 'all_off' ? 'Tout OFF' : config?.scenes?.find(scene => scene.id === id)?.name || `Scène retirée : ${id}`;
    function summary(item, key) {
        if (key === 'schedules') return `${item.days.map(day => days[day]).join(', ')} · ${item.time} · ${sceneName(item.scene_id)}`;
        const pin = config.io_pins.find(pin => pin.gpio === item.gpio), analog = pin?.type === 'ANALOG_INPUT';
        return `${pin?.label || `GPIO ${item.gpio}`} ${analog ? `${item.operator === 'below' ? '<' : '>'} ${item.threshold} ${pin.calibration?.unit || 'ADC'} · marge ${item.hysteresis}` : item.operator === 'high' ? '= HIGH' : '= LOW'} → ${sceneName(item.scene_id)} · stable ${item.stable_ms / 1000} s · délai ${item.cooldown_ms / 1000} s${item.window ? ` · ${item.window.start}–${item.window.end} (${draft.timezone})` : ''}`;
    }
    function render() {
        const changed = dirty(), error = config ? M.validate(candidate()) : '';
        ['templateDaily', 'templateThreshold', 'templateDigital'].forEach(id => $(id).disabled = busy || !config || !online);
        $('saveBar').hidden = !changed; $('draftSummary').textContent = `${draft.schedules.length} horaire(s) · ${draft.rules.length} règle(s) · ${draft.paused ? 'en pause' : 'exécution active'} · ${draft.timezone}`;
        $('draftError').textContent = error;
        $('saveButton').disabled = !online || busy || conflict || !!error || !changed || !!editing;
        $('discardButton').disabled = busy; $('timezone').disabled = busy || !config;
        if (document.activeElement !== $('timezone')) $('timezone').value = draft.timezone;
        $('pauseButton').disabled = !online || busy || changed || !!editing;
        $('pauseButton').textContent = runtime?.paused !== false ? 'Reprendre les automatismes' : 'Mettre en pause';
        $('newSchedule').disabled = !online || busy || draft.schedules.length >= 12; $('newRule').disabled = !online || busy || draft.rules.length >= 12 || !config?.io_pins.some(pin => !M.output(pin));
        $('conflictPanel').hidden = !conflict;
        if (conflict) $('conflictDetail').textContent = `Board : ${JSON.stringify(config.automation || M.defaultAutomation(), null, 2)}\nBrouillon : ${JSON.stringify(draft, null, 2)}`;
        for (const key of ['schedules', 'rules']) {
            const list = $(key === 'schedules' ? 'scheduleCards' : 'ruleCards'); list.replaceChildren();
            $(key === 'schedules' ? 'scheduleEmpty' : 'ruleEmpty').hidden = draft[key].length > 0;
            draft[key].forEach(item => {
                const card = document.createElement('article'); card.className = 'control-card'; card.dataset.id = item.id; if (item.id === new URLSearchParams(location.search).get('id')) card.classList.add('linked-item');
                const title = document.createElement('h3'); title.textContent = item.name;
                const note = document.createElement('p'); note.className = 'panel-note'; note.textContent = summary(item, key);
                const state = runtime?.items.find(state => state.id === item.id), status = document.createElement('p'); status.className = 'control-status';
                status.textContent = changed ? 'Brouillon · enregistrement en attente' : !online ? 'État indisponible' : stateNames[state?.state] || 'En attente';
                if (!changed && online && state?.next_epoch) status.textContent += ' · prochaine : ' + new Date(state.next_epoch * 1000).toLocaleString('fr-CA', { timeZone: draft.timezone });
                if (!changed && online && state?.value != null) status.textContent += ' · lecture : ' + U.number(state.value);
                if (!changed && online && state?.count) status.textContent += ` · ${state.count} tentative(s) · ${state.message}`;
                const button = document.createElement('button'); button.className = 'btn secondary'; button.textContent = 'Modifier'; button.disabled = busy;
                button.addEventListener('click', () => openEditor(key, item)); const detail = document.createElement('p'); detail.className = 'panel-note'; if (key === 'rules') detail.textContent = changed ? 'Enregistrez pour suivre l’exécution de ce brouillon.' : !online ? 'Lecture indisponible.' : M.explainRule(item, state);
                const duplicate = document.createElement('button'); duplicate.className = 'btn secondary'; duplicate.textContent = 'Dupliquer'; duplicate.disabled = busy || draft[key].length >= 12; duplicate.addEventListener('click', () => { const copy = { ...clone(item), id: M.newId(key === 'rules' ? 'rule' : 'schedule'), name: M.copyName(item.name), enabled: false }; draft[key].push(copy); render(); U.notice('Copie désactivée ajoutée au brouillon.'); });
                const actions = document.createElement('div'); actions.className = 'toolbar'; actions.append(button, duplicate); card.append(title, note, status, detail, actions); list.append(card);
            });
        }
        $('runList').replaceChildren();
        (runtime?.runs || []).forEach(run => {
            const li = document.createElement('li'); li.dataset.state = run.ok ? 'ok' : 'error';
            li.textContent = `${run.source === 'rule' ? 'Règle' : 'Horaire'} · ${run.name} · ${run.message} · ${U.age((runtime.now_ms - run.at_ms) >>> 0)}`; $('runList').append(li);
        });
    }
    function ruleOptions() {
        const pin = config.io_pins.find(pin => pin.gpio === Number($('itemGPIO').value)), analog = pin?.type === 'ANALOG_INPUT', old = $('itemOperator').value;
        $('itemOperator').replaceChildren(...(analog ? [['below', 'Sous le seuil'], ['above', 'Au-dessus du seuil']] : [['high', 'HIGH'], ['low', 'LOW']]).map(([value, label]) => new Option(label, value)));
        if ([...$('itemOperator').options].some(option => option.value === old)) $('itemOperator').value = old;
        $('thresholdField').hidden = $('hysteresisField').hidden = !analog;
        $('rulePreview').textContent = `${pin?.label || ''} · ${pin?.calibration?.unit || (analog ? 'ADC brut' : 'niveau électrique')} · La scène doit utiliser une durée pour chaque action ON.`;
    }
    function openEditor(key, item = null) {
        if (!config || busy) return;
        $('simulationPanel').hidden = true;
        editing = { key, id: item?.id || `${key === 'rules' ? 'rule' : 'schedule'}_${Date.now().toString(36)}`, existing: !!item };
        $('editor').hidden = false; $('editorTitle').textContent = key === 'schedules' ? 'Configurer un horaire' : 'Configurer une règle';
        $('itemName').value = item?.name || (key === 'schedules' ? 'Nouvel horaire' : 'Nouvelle règle'); $('itemEnabled').checked = item?.enabled || false;
        $('itemScene').replaceChildren(new Option('Tout OFF', 'all_off'), ...(config.scenes || []).map(scene => new Option(scene.name, scene.id)));
        if (item?.scene_id && ![...$('itemScene').options].some(option => option.value === item.scene_id)) $('itemScene').add(new Option(`Scène retirée : ${item.scene_id}`, item.scene_id));
        $('itemScene').value = item?.scene_id || config.scenes?.[0]?.id || 'all_off';
        $('scheduleFields').hidden = key !== 'schedules'; $('ruleFields').hidden = key !== 'rules';
        $('itemTime').value = item?.time || '08:00'; $('itemDays').replaceChildren();
        days.forEach((name, day) => { const label = document.createElement('label'); label.className = 'check'; const input = document.createElement('input'); input.type = 'checkbox'; input.value = day; input.checked = (item?.days || [1, 2, 3, 4, 5]).includes(day); label.append(input, name); $('itemDays').append(label); });
        $('itemGPIO').replaceChildren(...config.io_pins.filter(pin => !M.output(pin)).map(pin => new Option(`${pin.label} · GPIO ${pin.gpio}${pin.zone ? ' · ' + pin.zone : ''}`, pin.gpio)));
        if (item?.gpio != null) $('itemGPIO').value = item.gpio;
        $('itemThreshold').value = item?.threshold ?? 30; $('itemHysteresis').value = item?.hysteresis ?? 5; $('itemStable').value = (item?.stable_ms ?? 2000) / 1000; $('itemCooldown').value = (item?.cooldown_ms ?? 60000) / 1000;
        $('itemWindow').checked = !!item?.window; $('windowStart').value = item?.window?.start || '08:00'; $('windowEnd').value = item?.window?.end || '20:00'; windowFields();
        ruleOptions(); if (item?.operator) $('itemOperator').value = item.operator;
        $('deleteItem').disabled = !item; render(); $('editor').scrollIntoView({ behavior: 'smooth' }); $('itemName').focus();
    }
    function closeEditor() { editing = null; $('editor').hidden = true; render(); }
    async function refresh() {
        const results = await Promise.allSettled([U.json('/api/io/config'), U.json('/api/io/automation/status')]);
        if (results.some(result => result.status !== 'fulfilled')) { online = false; U.badge('boardBadge', 'Board inaccessible', 'error'); U.badge('automationBadge', 'État indisponible', 'warning'); $('clockStatus').textContent = 'Heure inconnue'; render(); return; }
        const next = results[0].value, previous = config;
        if (!config || !dirty()) draft = clone(next.automation || M.defaultAutomation());
        else if (M.signature(previous.automation || M.defaultAutomation()) !== M.signature(next.automation || M.defaultAutomation()) && M.signature(draft) !== M.signature(next.automation || M.defaultAutomation())) conflict = true;
        config = next; runtime = results[1].value; online = true;
        U.badge('boardBadge', 'Board en ligne', 'ok'); U.badge('automationBadge', runtime.paused ? 'Automatismes en pause' : 'Automatismes actifs', runtime.paused ? 'warning' : 'ok');
        $('clockStatus').textContent = runtime.clock_valid ? `${runtime.local_time} · ${runtime.timezone}` : 'Heure indisponible · horaires et règles avec plage suspendus'; render();
    }
    async function save(value = draft) {
        if (!config || busy || !online) return;
        const next = { ...config, automation: clone(value) }, error = M.validate(next); if (error) { U.notice(error, true); return; }
        busy = true; render();
        try { await U.json('/api/io/config', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(next) }); config = await U.json('/api/io/config'); draft = clone(config.automation || M.defaultAutomation()); conflict = false; U.notice('Automatismes enregistrés sur le board.'); }
        catch (e) { U.notice(`Enregistrement non confirmé : ${e.message}`, true); }
        finally { busy = false; await refresh(); }
    }
    function editorItem() {
        const { key, id } = editing, item = { id, name: $('itemName').value.trim(), scene_id: $('itemScene').value, enabled: $('itemEnabled').checked };
        if (key === 'schedules') { item.time = $('itemTime').value; item.days = [...$('itemDays').querySelectorAll('input:checked')].map(input => Number(input.value)); }
        else { item.gpio = Number($('itemGPIO').value); item.operator = $('itemOperator').value; const number = id => $(id).value === '' ? NaN : Number($(id).value); item.threshold = number('itemThreshold'); item.hysteresis = number('itemHysteresis'); item.stable_ms = Math.round(number('itemStable') * 1000); item.cooldown_ms = Math.round(number('itemCooldown') * 1000); if ($('itemWindow').checked) item.window = { start: $('windowStart').value, end: $('windowEnd').value }; }
        return item;
    }
    function editorCandidate() { const next = clone(draft), item = editorItem(), index = next[editing.key].findIndex(entry => entry.id === item.id); if (index < 0) next[editing.key].push(item); else next[editing.key][index] = item; return { config: { ...config, automation: next }, item }; }
    $('automationForm').addEventListener('submit', event => {
        event.preventDefault(); if (!editing || busy) return;
        const next = editorCandidate(), error = M.validate(next.config); if (error) return U.notice(error, true);
        draft = next.config.automation; closeEditor(); U.notice('Modification dans le brouillon. Enregistrez pour l’appliquer.');
    });
    $('previewItem').addEventListener('click', async () => {
        if (!editing || busy || !online) return;
        const next = editorCandidate(), error = M.validate(next.config); if (error) return U.notice(error, true);
        $('previewItem').disabled = true;
        try {
            const p = await U.json('/api/io/automation/preview', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ config: next.config, id: next.item.id }) });
            const detail = [];
            if ('condition_now' in p) { detail.push(p.value == null ? 'Lecture indisponible.' : `Lecture ${U.number(p.value)} · condition ${p.condition_now ? 'atteinte' : 'non atteinte'}.`); detail.push(`Stabilité ${p.stable_ms / 1000} s · délai entre déclenchements ${p.cooldown_ms / 1000} s.`); detail.push('Une lecture initiale déjà vraie ne déclenche rien. Un nouveau franchissement stable est nécessaire.'); }
            else detail.push(p.next_epoch ? 'Prochaine occurrence : ' + new Date(p.next_epoch * 1000).toLocaleString('fr-CA', { timeZone: next.config.automation.timezone }) : 'Heure valide indisponible.');
            if ('window_allowed' in p) detail.push(`Plage ${next.item.window.start}–${next.item.window.end} (${draft.timezone}) : ${!p.clock_valid ? 'heure indisponible' : p.window_allowed ? 'ouverte maintenant' : 'fermée maintenant'}. Les franchissements bloqués ne sont pas rattrapés.`);
            if (p.paused || !p.enabled) detail.push(`${p.paused ? 'Automatismes en pause.' : ''} ${!p.enabled ? 'Entrée désactivée.' : ''}`.trim());
            if (p.override_remaining_ms) detail.push(`Priorité manuelle : encore ${Math.ceil(p.override_remaining_ms / 1000)} s.`);
            detail.push(p.targets_available ? 'Sorties actuellement disponibles.' : `Scène indisponible : ${p.target_reason}`);
            detail.push(...p.outputs.map(action => `${config.io_pins.find(pin => pin.gpio === action.gpio)?.label || `GPIO ${action.gpio}`} → ${action.active ? 'ON' : 'OFF'}${action.duration_ms ? ` pendant ${action.duration_ms / 1000} s` : ''}`));
            $('simulationResult').textContent = detail.join(' '); $('simulationPanel').hidden = false;
        } catch (e) { U.notice(`Aperçu indisponible : ${e.message}`, true); }
        finally { $('previewItem').disabled = false; }
    });
    function fromTemplate(kind) {
        if (!config || busy) return;
        if (kind === 'daily') { openEditor('schedules'); $('itemName').value = 'Éclairage quotidien'; $('itemDays').querySelectorAll('input').forEach(input => input.checked = true); return; }
        const analog = kind === 'threshold', pin = config.io_pins.find(pin => !M.output(pin) && (pin.type === 'ANALOG_INPUT') === analog);
        if (!pin) return U.notice(analog ? 'Configurez d’abord une entrée analogique dans la mise en service.' : 'Configurez d’abord une entrée numérique.', true);
        openEditor('rules'); $('itemGPIO').value = pin.gpio; ruleOptions(); $('itemName').value = analog ? 'Ventilation sur seuil' : 'Entrée active'; $('itemOperator').value = analog ? 'above' : 'high';
    }
    $('templateDaily').addEventListener('click', () => fromTemplate('daily')); $('templateThreshold').addEventListener('click', () => fromTemplate('threshold')); $('templateDigital').addEventListener('click', () => fromTemplate('digital'));
    $('timezone').addEventListener('change', () => { draft.timezone = $('timezone').value; render(); });
    $('newSchedule').addEventListener('click', () => openEditor('schedules')); $('newRule').addEventListener('click', () => openEditor('rules'));
    function windowFields() { $('windowFields').hidden = !$('itemWindow').checked; $('windowStart').disabled = $('windowEnd').disabled = !$('itemWindow').checked; }
    $('itemWindow').addEventListener('change', windowFields);
    $('itemGPIO').addEventListener('change', ruleOptions); $('closeEditor').addEventListener('click', closeEditor);
    $('deleteItem').addEventListener('click', () => { draft[editing.key] = draft[editing.key].filter(item => item.id !== editing.id); closeEditor(); });
    $('saveButton').addEventListener('click', () => save());
    $('pauseButton').addEventListener('click', () => { if (!dirty() && !editing) save({ ...draft, paused: !draft.paused }); });
    $('discardButton').addEventListener('click', () => { draft = clone(config.automation || M.defaultAutomation()); conflict = false; closeEditor(); });
    $('useBoard').addEventListener('click', () => $('discardButton').click()); $('keepDraft').addEventListener('click', () => { conflict = false; render(); });
    const poll = U.poll(refresh, 2500); $('refreshButton').addEventListener('click', poll);
    window.addEventListener('beforeunload', event => { if (dirty() || editing) { event.preventDefault(); event.returnValue = ''; } });
})();
