(() => {
    const U = BoardUI, M = IOModel, $ = id => document.getElementById(id), clone = value => JSON.parse(JSON.stringify(value));
    let config = null, statuses = null, board = null, online = false, busy = false, step = 1, target = null, calibrationDirty = false, previousValue = null, changes = 0, testStarted = 0, testConfirmed = false;
    const gpio = () => Number($('wizardGPIO').value), pin = () => config?.io_pins.find(pin => pin.gpio === gpio());
    const metadata = () => statuses?.pins.find(pin => pin.gpio === gpio());
    const reading = () => statuses?.statuses.find(pin => pin.gpio === gpio());
    const reserved = () => metadata()?.usages.some(owner => ['I2C', 'SERIAL'].includes(owner.mode));
    function show(next) {
        step = next;
        ['identifyPanel', 'configurePanel', 'verifyPanel'].forEach((id, index) => { $(id).hidden = index + 1 !== step; if (index + 1 === step) $('step' + (index + 1)).setAttribute('aria-current', 'step'); else $('step' + (index + 1)).removeAttribute('aria-current'); });
        if (next === 2) {
            target = clone(pin() || { gpio: gpio(), label: `GPIO ${gpio()}`, mode: 'INPUT', type: 'DIGITAL', initial_state: '', graph: false });
            $('pinTemplate').value = ''; $('wizardIcon').value = target.icon || ''; $('wizardName').value = target.label; $('wizardZone').value = target.zone || ''; $('wizardInverted').checked = !!target.inverted;
            $('wizardRole').value = target.mode === 'OUTPUT' ? 'output' : target.type === 'ANALOG_INPUT' ? 'analog' : target.mode === 'INPUT_PULLUP' ? 'pullup' : target.mode === 'INPUT_PULLDOWN' ? 'pulldown' : 'digital';
            $('configureTitle').textContent = `GPIO ${gpio()} · Nom et rôle`; roles();
        }
        if (next === 3) {
            previousValue = null; changes = 0; testStarted = 0; testConfirmed = false;
            $('advancedLink').href = `/io_control.html?gpio=${gpio()}`; const c = pin()?.calibration;
            $('referenceA').value = c?.raw_min ?? ''; $('referenceB').value = c?.raw_max ?? ''; $('valueA').value = c?.value_min ?? 0; $('valueB').value = c?.value_max ?? 100; $('wizardUnit').value = c?.unit ?? '%'; $('wizardClamp').checked = c?.clamp !== false; calibrationDirty = false;
            $('outputTestResult').textContent = ''; $('inputTestResult').textContent = 'En attente d’un changement.';
        }
        update();
    }
    function roles() {
        const options = $('wizardRole').options, only = M.INPUT_ONLY.includes(gpio());
        for (const option of options) option.disabled = reserved() || (only && ['output', 'pullup', 'pulldown'].includes(option.value)) || option.value === 'analog' && !M.ADC.includes(gpio());
        if ($('wizardRole').selectedOptions[0]?.disabled && !reserved()) $('wizardRole').value = 'digital';
        $('invertField').hidden = $('wizardRole').value !== 'output';
        $('roleNote').textContent = $('wizardRole').value === 'output' ? 'Une nouvelle sortie démarre OFF. Une sortie existante conserve son état logique ; un test de 1 s permet de vérifier le câblage.' : $('wizardRole').value === 'analog' && M.ADC2.includes(gpio()) ? 'ADC2 indisponible pendant le Wi-Fi. Choisissez une entrée ADC1 libre, par exemple GPIO32 ou GPIO33.' : only ? 'Entrée uniquement, sans résistance pull-up/down interne.' : 'L’éditeur complet reste disponible après cette étape.';
    }
    function calibration() {
        const number = id => $(id).value === '' ? NaN : Number($(id).value);
        return { raw_min: number('referenceA'), raw_max: number('referenceB'), value_min: number('valueA'), value_max: number('valueB'), unit: $('wizardUnit').value, clamp: $('wizardClamp').checked };
    }
    function update() {
        const p = pin(), r = online ? reading() : null, info = metadata(), analog = p?.type === 'ANALOG_INPUT', output = M.output(p), raw = r?.type === 'ANALOG_INPUT' && typeof r.value === 'number' ? r.value : null;
        $('identifyNext').disabled = !config || !online || busy || reserved(); $('wizardSave').disabled = !online || busy || reserved();
        $('pinCapabilities').textContent = `${M.INPUT_ONLY.includes(gpio()) ? 'Entrée uniquement' : 'Entrée / sortie'}${M.ADC.includes(gpio()) ? ' · ADC' : ''}${p ? ' · ' + p.label : ' · non configurée'}`;
        $('pinOwners').textContent = info?.usages.length ? 'Usages déclarés : ' + info.usages.map(owner => owner.label).join(', ') : 'Aucun usage réservé déclaré.';
        if (reserved()) $('pinOwners').textContent += ' · Configuration guidée indisponible pour cet usage.';
        $('verifyTitle').textContent = `${p?.label || 'Lecture'} · GPIO ${gpio()}`;
        $('wizardReading').textContent = r?.value == null ? 'Indisponible' : output ? r.active ? 'ON' : 'OFF' : analog ? `${U.number(r.calibrated_value ?? r.value)} ${r.unit || 'ADC'}` : r.value ? 'HIGH' : 'LOW';
        $('readingDescription').textContent = !online ? 'Board inaccessible · commandes suspendues' : r?.value == null ? r?.reason || r?.value_error || 'En attente de lecture' : analog ? `ADC brut : ${r.value}` : `Niveau électrique : ${r.value ? 'HIGH' : 'LOW'}${r.remaining_ms ? ` · arrêt dans ${Math.ceil(r.remaining_ms / 1000)} s` : ''}`;
        $('outputTest').hidden = !output; $('inputTest').hidden = output; $('calibrationPanel').hidden = !analog;
        $('testOutput').disabled = !online || busy || r?.value == null; $('stopOutput').disabled = !online || busy || r?.value == null;
        $('captureA').disabled = $('captureB').disabled = !online || busy || raw == null;
        const c = calibration(), error = M.calibrationError(c); $('saveCalibration').disabled = !online || busy || raw == null || !!error;
        $('calibrationPreview').textContent = error || (raw == null ? 'Attente d’une lecture analogique disponible.' : `Aperçu : ${U.number(M.convert(raw, c))} ${c.unit} · ADC brut ${raw}`);
        if (step === 3 && !output && r?.value != null) { if (previousValue != null && previousValue !== r.value) { changes++; $('inputTestResult').textContent = `${changes} changement(s) observé(s) · ${previousValue} → ${r.value}`; } previousValue = r.value; }
        if (step === 3 && output && testStarted && Date.now() - testStarted > 1500 && r?.value != null && !r.active && !r.remaining_ms) { $('outputTestResult').textContent = testConfirmed ? 'ON confirmé par le board, puis retour OFF confirmé. Vérifiez aussi le composant physique.' : 'Retour OFF confirmé ; la lecture ON n’a pas été observée.'; testStarted = 0; }
        $('wizardForm').querySelectorAll('input, select').forEach(input => { input.disabled = busy || !online; });
    }
    async function apply(changedPin) {
        const next = { ...config, io_pins: config.io_pins.some(pin => pin.gpio === gpio()) ? config.io_pins.map(pin => pin.gpio === gpio() ? changedPin : pin) : [...config.io_pins, changedPin] }, error = M.validate(next);
        if (error) throw new Error(error);
        await U.json('/api/io/config', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(next) }); config = await U.json('/api/io/config');
    }
    async function refresh() {
        const results = await Promise.allSettled([U.json('/api/io/config'), U.json('/api/io/status'), U.json('/api/status')]);
        if (results.some(result => result.status !== 'fulfilled')) { online = false; U.badge('boardBadge', 'Board inaccessible', 'error'); update(); return; }
        const next = results[0].value;
        // Keep the configuration revision of an active form. Its save gets a 409 if changed elsewhere.
        if (!config || step === 1 || step === 3 && !calibrationDirty) config = next;
        statuses = results[1].value; board = results[2].value; online = true; U.badge('boardBadge', 'Board en ligne', 'ok'); $('boardIdentity').textContent = `${board.device_name} · ${board.wifi_ip}`;
        if (!$('wizardGPIO').options.length) { $('wizardGPIO').replaceChildren(...M.GPIOS.map(gpio => new Option(`GPIO ${gpio}${config.io_pins.find(pin => pin.gpio === gpio)?.label ? ' · ' + config.io_pins.find(pin => pin.gpio === gpio).label : ''}`, gpio))); const requested = Number(new URLSearchParams(location.search).get('gpio')); if (new URLSearchParams(location.search).has('gpio') && M.GPIOS.includes(requested)) $('wizardGPIO').value = requested; }
        update();
    }
    $('wizardIcon').replaceChildren(...M.iconOptions());
    $('pinTemplate').addEventListener('change', () => {
        const template = M.pinTemplates[$('pinTemplate').value]; if (!template) return;
        if (M.INPUT_ONLY.includes(gpio()) && ['OUTPUT', 'INPUT_PULLUP'].includes(template.mode) || template.type === 'ANALOG_INPUT' && !M.ADC.includes(gpio())) { $('pinTemplate').value = ''; return U.notice('Ce modèle nécessite une autre broche. Choisissez un GPIO compatible.', true); }
        $('wizardName').value = template.label; $('wizardIcon').value = template.icon;
        $('wizardRole').value = template.mode === 'OUTPUT' ? 'output' : template.type === 'ANALOG_INPUT' ? 'analog' : template.mode === 'INPUT_PULLUP' ? 'pullup' : 'digital';
        target = { ...target, ...template }; if (template.mode !== 'OUTPUT') delete target.pulse_presets_ms;
        if (template.type !== 'ANALOG_INPUT') delete target.calibration;
        roles(); U.notice('Modèle préparé. Enregistrez pour appliquer le rôle ; vérifiez ensuite le câblage et la calibration.');
    });
    $('wizardGPIO').addEventListener('change', update); $('identifyNext').addEventListener('click', () => show(2)); $('backIdentify').addEventListener('click', () => show(1)); $('backConfigure').addEventListener('click', () => show(2)); $('anotherPin').addEventListener('click', () => show(1)); $('wizardRole').addEventListener('change', roles);
    $('wizardForm').addEventListener('submit', async event => {
        event.preventDefault(); if (busy || !online || reserved()) return;
        const role = $('wizardRole').value, p = { ...target, gpio: gpio(), label: $('wizardName').value.trim(), icon: $('wizardIcon').value, zone: $('wizardZone').value.trim(), mode: ({ output: 'OUTPUT', pullup: 'INPUT_PULLUP', pulldown: 'INPUT_PULLDOWN' })[role] || 'INPUT', type: role === 'analog' ? 'ANALOG_INPUT' : 'DIGITAL' };
        p.inverted = role === 'output' && $('wizardInverted').checked;
        if (role !== 'analog') delete p.calibration; if (role !== 'output') { delete p.pulse_presets_ms; p.initial_state = ''; } else if (!M.output(pin())) p.initial_state = p.inverted ? 'HIGH' : 'LOW';
        busy = true; update();
        try { await apply(p); await refresh(); show(3); U.notice('Configuration enregistrée. Vérifiez maintenant la lecture ou le composant.'); } catch (e) { U.notice(`Enregistrement non confirmé : ${e.message}. Revenir à Identifier permet de recharger la configuration.`, true); } finally { busy = false; update(); }
    });
    async function outputCommand(on, duration = 0) {
        if (!online || busy) return; busy = true; update();
        try { const result = await U.json(`/api/io/set?gpio=${gpio()}&active=${on ? 1 : 0}${duration ? `&duration_ms=${duration}` : ''}`, { method: 'POST' }); if (on && duration) { testStarted = Date.now(); testConfirmed = result.active === true; $('outputTestResult').textContent = 'ON confirmé · attente du retour OFF…'; } else { testStarted = 0; $('outputTestResult').textContent = 'OFF confirmé.'; } await refresh(); }
        catch (e) { U.notice(`Commande non confirmée : ${e.message}`, true); } finally { busy = false; update(); }
    }
    $('testOutput').addEventListener('click', () => outputCommand(true, 1000)); $('stopOutput').addEventListener('click', () => outputCommand(false));
    ['A', 'B'].forEach(ref => $('capture' + ref).addEventListener('click', () => { const r = reading(); if (r?.type === 'ANALOG_INPUT' && typeof r.value === 'number') { $('reference' + ref).value = r.value; calibrationDirty = true; update(); } }));
    $('calibrationForm').querySelectorAll('input').forEach(input => input.addEventListener('input', () => { calibrationDirty = true; update(); }));
    $('calibrationForm').addEventListener('submit', async event => { event.preventDefault(); if (!online || busy || !pin() || pin().type !== 'ANALOG_INPUT') return; const c = calibration(); if (M.calibrationError(c)) return; busy = true; update(); try { await apply({ ...pin(), calibration: c }); calibrationDirty = false; U.notice('Calibration enregistrée, ADC brut conservé.'); await refresh(); } catch (e) { U.notice(`Calibration non confirmée : ${e.message}`, true); } finally { busy = false; update(); } });
    U.poll(refresh, 700);
    window.addEventListener('beforeunload', event => { if (calibrationDirty || step === 2) { event.preventDefault(); event.returnValue = ''; } });
})();
