// Fixture acceptance for the I/O page; no device writes.
const assert = require('node:assert/strict');
const http = require('node:http');
const fs = require('node:fs');
const path = require('node:path');
const { chromium } = require('playwright');
const root = path.resolve(__dirname, '../data');
const M = require('../data/components/io-model.js');
const artifact = process.env.UI_ARTIFACT_DIR || '/tmp/iyak32-io-ui';
fs.mkdirSync(artifact, { recursive: true });
let revision = 1; const timers = {};
let offline = false, rejectCommand = false, rejectSave = false, journalOffline = false, lastSave;
let validationCount = 0, configurationSaves = 0, sceneRequests = 0;
let session = 'fixture-boot', eventSequence = 0; const journal = [];
const journalEvent = (pin, source, kind, previous, duration_ms = 0, scene = '') => {
    journal.push({ id: ++eventSequence, at_ms: Date.now() >>> 0, gpio: pin.gpio, label: pin.label, source, kind, previous, value: values[pin.gpio], active: !!values[pin.gpio] !== !!pin.inverted, output: pin.mode === 'OUTPUT', duration_ms, ...(scene ? { scene } : {}) });
    if (journal.length > 64) journal.shift();
    return eventSequence;
};
let config = { device_note: 'Preserve me', io_pins: [
    { gpio: 12, label: 'Bouton', mode: 'INPUT_PULLUP', type: 'DIGITAL', initial_state: '', graph: false },
    { gpio: 34, label: 'Entrée legacy', mode: 'INPUT_PULLUP', type: 'DIGITAL', initial_state: '', graph: false },
    { gpio: 39, label: 'Capteur zéro', mode: 'INPUT', type: 'ANALOG_INPUT', initial_state: '', graph: true },
    { gpio: 25, label: 'Capteur ADC2', mode: 'INPUT', type: 'ANALOG_INPUT', initial_state: '', graph: false },
    { gpio: 15, label: 'LED test', mode: 'OUTPUT', type: 'DIGITAL', initial_state: 'LOW', graph: false },
    { gpio: 4, label: 'LED active', mode: 'OUTPUT', type: 'DIGITAL', initial_state: 'HIGH', graph: false }
] };
const values = { 12: 1, 34: 0, 39: 0, 25: null, 15: 0, 4: 1 };
const commands = [];
function expireFixturePulse(gpio, pin, deadline, scene = '') {
    if (timers[gpio] !== deadline) return;
    const left = deadline - Date.now();
    if (left > 0) { setTimeout(() => expireFixturePulse(gpio, pin, deadline, scene), left); return; }
    values[gpio] = Number(!!pin.inverted); timers[gpio] = 0;
    journalEvent(pin, 'timer', 'expired', 1, 0, scene);
}
const server = http.createServer((request, response) => {
    const url = new URL(request.url, 'http://localhost');
    const reply = (data, code = 200) => { response.writeHead(code, { 'Content-Type': 'application/json' }); response.end(JSON.stringify(data)); };
    if (url.pathname.startsWith('/api/')) {
        if (offline) return reply({ error: 'Board unavailable' }, 503);
        if (url.pathname === '/api/status') return reply({ mode: 'basic' });
        if (url.pathname === '/api/mqtt/status') return reply({ connected: true, io_discovery_ready: true, io_discovery_count: config.io_pins.length });
        if (url.pathname === '/api/io/events') {
            if (journalOffline) return reply({ error: 'Journal unavailable' }, 503);
            let after = Number(url.searchParams.get('after'));
            const reset = !!url.searchParams.get('session') && url.searchParams.get('session') !== session || after > eventSequence;
            if (reset) after = 0;
            return reply({ session, latest_id: eventSequence, now_ms: Date.now() >>> 0, capacity: 64, reset,
                lost: after > 0 && journal.length ? Math.max(0, journal[0].id - after - 1) : 0, events: journal.filter(event => event.id > after) });
        }
        if (url.pathname === '/api/io/config/validate' || url.pathname === '/api/io/scene') {
            let body = ''; request.on('data', data => { body += data; }); request.on('end', () => {
                const input = JSON.parse(body);
                if (url.pathname.endsWith('/validate')) { validationCount++; const error = M.validate(input); return reply(error ? { message: error } : { status: 'valid' }, error ? 400 : 200); }
                sceneRequests++;
                if (input._revision !== `fixture.${revision}`) return reply({ message: 'Configuration modifiée ailleurs' }, 409);
                const scene = input.id === 'all_off' ? { id: 'all_off', name: 'Tout OFF', actions: config.io_pins.filter(pin => M.output(pin) && pin.gpio !== 21).map(pin => ({ gpio: pin.gpio, active: false })) } : (config.scenes || []).find(scene => scene.id === input.id);
                if (!scene || scene.actions.some(action => action.gpio === 21 || !M.output(config.io_pins.find(pin => pin.gpio === action.gpio)))) return reply({ message: 'Sortie indisponible : aucune commande exécutée.' }, 400);
                const outputs = scene.actions.map(action => {
                    const pin = config.io_pins.find(pin => pin.gpio === action.gpio), previous = !!values[action.gpio] !== !!pin.inverted;
                    values[action.gpio] = Number(action.active !== !!pin.inverted);
                    const ms = action.duration_ms || 0; timers[action.gpio] = ms ? Date.now() + ms : 0; const deadline = timers[action.gpio];
                    journalEvent(pin, 'web', ms ? 'pulse' : 'command', Number(previous), ms, scene.name);
                    if (ms) setTimeout(() => expireFixturePulse(action.gpio, pin, deadline, scene.name), ms);
                    return { gpio: action.gpio, active: action.active, value: values[action.gpio], remaining_ms: ms };
                });
                return reply({ status: 'success', id: scene.id, name: scene.name, outputs, skipped: input.id === 'all_off' && config.io_pins.some(pin => pin.gpio === 21) ? [21] : [], event_id: eventSequence });
            }); return;
        }
        if (url.pathname === '/api/io/config' && request.method === 'POST') {
            let body = ''; request.on('data', data => { body += data; }); request.on('end', () => {
                if (rejectSave) return reply({ message: 'Enregistrement refusé' }, 400);
                lastSave = JSON.parse(body);
                const error = M.validate(lastSave); if (error) return reply({ message: error }, 400);
                if (lastSave._revision !== `fixture.${revision}`) return reply({ message: 'Configuration modifiée ailleurs' }, 409);
                lastSave.io_pins.forEach(pin => {
                    const old = config.io_pins.find(old => old.gpio === pin.gpio);
                    if (old?.mode === 'OUTPUT' && pin.mode === 'OUTPUT' && !!old.inverted !== !!pin.inverted)
                        values[pin.gpio] = Number((!!values[pin.gpio] !== !!old.inverted) !== !!pin.inverted);
                });
                config = lastSave; delete config._revision; revision++; configurationSaves++; return reply({ status: 'success' });
            }); return;
        }
        if (url.pathname === '/api/io/config') return reply({ ...config, _revision: `fixture.${revision}` });
        if (url.pathname === '/api/io/status') return reply({ _revision: `fixture.${revision}`, pins: [{ gpio: 35, usages: [{ label: 'Mesure de batterie', mode: 'ANALOG_INPUT' }] }, { gpio: 21, usages: [{ label: 'I²C · SDA', mode: 'I2C' }] }], statuses: config.io_pins.map(pin => ({ ...pin,
            mode: [34, 35, 36, 39].includes(pin.gpio) ? 'INPUT' : pin.mode,
            active: !!values[pin.gpio] !== !!pin.inverted, remaining_ms: Math.max(0, (timers[pin.gpio] || 0) - Date.now()), value: pin.gpio === 21 ? null : values[pin.gpio] ?? (pin.gpio === 25 ? null : 0), ...(pin.gpio === 21 ? { active: null, value_error: 'PIN_IN_USE', reason: 'I²C · SDA' } : {}), ...(pin.gpio === 25 ? { value_error: 'ADC2_WIFI' } : {}) })) });
        if (url.pathname === '/api/io/set') {
            const gpio = Number(url.searchParams.get('gpio')), value = Number(url.searchParams.get('active'));
            commands.push({ gpio, value });
            if (rejectCommand) return reply({ message: 'Commande refusée' }, 400);
            assert(config.io_pins.some(pin => pin.gpio === gpio && pin.mode === 'OUTPUT'));
            const pin = config.io_pins.find(pin => pin.gpio === gpio);
            values[gpio] = Number(!!value !== !!pin.inverted);
            const duration = Number(url.searchParams.get('duration_ms'));
            const event_id = journalEvent(pin, 'web', duration ? 'pulse' : 'command', undefined, duration);
            timers[gpio] = duration ? Date.now() + duration : 0;
            const deadline = timers[gpio];
            if (duration) setTimeout(() => expireFixturePulse(gpio, pin, deadline), duration);
            return reply({ status: 'success', gpio, value: values[gpio], active: !!value, remaining_ms: duration, event_id });
        }
        return reply({ error: 'Not found' }, 404);
    }
    const file = path.join(root, url.pathname === '/' ? 'index.html' : url.pathname);
    if (!file.startsWith(root + path.sep) || !fs.existsSync(file)) { response.writeHead(404); response.end(); return; }
    response.setHeader('Content-Type', file.endsWith('.js') ? 'text/javascript' : file.endsWith('.css') ? 'text/css' : 'text/html');
    response.end(fs.readFileSync(file));
});
(async () => {
    await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
    const base = `http://127.0.0.1:${server.address().port}`;
    const browser = await chromium.launch({ headless: true, ...(process.env.CHROMIUM_PATH ? { executablePath: process.env.CHROMIUM_PATH } : {}) });
    try {
        const context = await browser.newContext({ viewport: { width: 1440, height: 1050 }, acceptDownloads: true });
        const page = await context.newPage(), errors = [], external = [];
        page.on('pageerror', error => errors.push(error.message)); page.on('request', r => { if (!r.url().startsWith(base)) external.push(r.url()); });
        const row = gpio => page.locator(`.pin-row[data-gpio="${gpio}"]`);
        const edit = async gpio => { await row(gpio).locator('.edit-button').click(); return page.locator(`#editor-${gpio}`); };
        const filtered = () => page.locator('.pin-row:visible');
        const waitOnline = () => page.waitForFunction(() => document.getElementById('boardBadge').textContent === 'Board en ligne');
        const noOverflow = async () => assert(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth), 'Horizontal overflow');
        await page.goto(base + '/io_control.html'); await waitOnline();
        assert.equal(await filtered().count(), 26); assert.equal(await page.locator('#configuredCount').textContent(), '6');
        assert.equal(await row(39).locator('.pin-state').textContent(), '0');
        assert.match(await row(25).locator('.pin-value-detail').textContent(), /ADC2/);
        assert.match(await row(34).locator('.pin-config-detail').textContent(), /effectif : Entrée/);
        assert.equal(await row(0).locator('.pin-state').textContent(), '—');
        assert.equal(await row(12).locator('.pin-commands').isVisible(), false);
        const card = gpio => page.locator(`.io-output-card[data-gpio="${gpio}"]`);
        assert.equal(await page.locator('.io-output-card').count(), 2);
        assert.equal(await page.locator('#activityList li').count(), 0);
        await card(15).locator('[data-value="1"]').click();
        await page.waitForFunction(() => document.querySelector('.io-output-card[data-gpio="15"] .output-card-state').textContent === 'ON');
        assert.equal(await row(15).locator('.pin-state').textContent(), 'ON');
        assert.match(await page.locator('.activity-event').first().textContent(), /Web · Commande confirmée/);
        await card(15).locator('[data-value="0"]').click();
        await page.waitForFunction(() => document.querySelector('.pin-row[data-gpio="15"] .pin-state').textContent === 'OFF');
        assert.equal(await card(15).locator('.output-card-state').textContent(), 'OFF');
        await card(15).locator('.favorite-button').click();
        assert.equal(await row(15).locator('.favorite-button').getAttribute('aria-pressed'), 'true');
        assert.equal(await page.locator('.io-output-card').first().getAttribute('data-gpio'), '15');
        assert.equal(await page.locator('#saveBar').isVisible(), false);
        await page.locator('[data-filter="favorites"]').click(); assert.equal(await filtered().count(), 1);
        await page.reload(); await waitOnline(); assert.equal(await filtered().count(), 1);
        assert.equal(await card(15).locator('.favorite-button').getAttribute('aria-pressed'), 'true');
        await page.waitForFunction(() => document.querySelectorAll('#activityList li').length === 2);
        assert.equal(await page.locator('#activityList li').count(), 2, 'Board journal survives page reload');
        await card(15).locator('.output-card-config').click();
        assert.equal(await page.locator('#editor-15').isVisible(), true);
        assert.equal(await page.locator('#editor-15 [data-field="label"]').evaluate(el => el === document.activeElement), true);
        await row(15).locator('.edit-button').click();
        await card(15).locator('.favorite-button').click(); assert.equal(await page.locator('#favoriteCount').textContent(), '0');
        values[15] = 1; journalEvent(config.io_pins.find(pin => pin.gpio === 15), 'mqtt', 'command', 0); await page.locator('#refreshButton').click();
        await page.waitForFunction(() => document.querySelector('.io-output-card[data-gpio="15"] .output-card-state').textContent === 'ON');
        await page.waitForFunction(() => document.querySelector('.activity-event')?.dataset.source === 'mqtt');
        assert.match(await page.locator('.activity-event').first().textContent(), /MQTT · Commande confirmée/);
        assert.equal(await page.locator('.activity-event').first().getAttribute('data-source'), 'mqtt');
        values[15] = 0; await page.locator('#refreshButton').click();
        await page.waitForFunction(() => document.querySelector('.pin-row[data-gpio="15"] .pin-state').textContent === 'OFF');
        await page.locator('#clearActivity').click(); assert.equal(await page.locator('#activityList li').count(), 0);
        await row(15).locator('[data-value="1"]').click(); await page.waitForFunction(() => document.querySelector('.pin-row[data-gpio="15"] .pin-state').textContent === 'ON');
        await row(15).locator('[data-value="1"]').click(); assert.equal(values[15], 1); assert.deepEqual(commands.slice(-2), [{ gpio: 15, value: 1 }, { gpio: 15, value: 1 }]);
        await row(15).locator('[data-value="0"]').click(); await page.waitForFunction(() => document.querySelector('.pin-row[data-gpio="15"] .pin-state').textContent === 'OFF');
        await page.locator('[data-filter="output"]').click(); assert.equal(await filtered().count(), 2);
        await page.locator('[data-filter="all"]').click(); await noOverflow();
        await page.screenshot({ path: path.join(artifact, 'io-desktop.png'), fullPage: true });
        await page.setViewportSize({ width: 390, height: 844 }); await noOverflow();
        await page.locator('[data-filter="configured"]').click(); await page.screenshot({ path: path.join(artifact, 'io-mobile.png'), fullPage: true });
        await page.locator('[data-filter="all"]').click(); const e34 = await edit(34);
        assert.equal(await e34.locator('[value="OUTPUT"]').evaluate(option => option.disabled), true);
        assert.equal(await e34.locator('[value="INPUT_PULLUP"]').evaluate(option => option.disabled), true); await noOverflow();
        await row(34).locator('.edit-button').click();
        const e15 = await edit(15); await e15.locator('[data-field="label"]').fill('LED renommée');
        assert.equal(await page.locator('#saveBar').isVisible(), true);
        assert.equal(await row(15).locator('[data-value="1"]').isDisabled(), true);
        assert.equal(await card(15).locator('[data-value="1"]').isDisabled(), true);
        assert.equal(await card(15).locator('.output-card-name').textContent(), 'LED test', 'Quick controls identify the active saved role');
        await page.locator('.io-change-preview summary').click();
        assert.match(await page.locator('#changeList').textContent(), /GPIO 15 · Nom : LED test → LED renommée/);
        await page.locator('#refreshButton').click(); await page.waitForTimeout(2200);
        assert.equal(await e15.locator('[data-field="label"]').inputValue(), 'LED renommée');
        rejectSave = true; await page.locator('#saveButton').click(); await page.waitForFunction(() => document.getElementById('actionNotice').textContent.includes('refusé'));
        assert.equal(await e15.locator('[data-field="label"]').inputValue(), 'LED renommée'); assert.equal(await e15.locator('[data-field="label"]').isDisabled(), false);
        rejectSave = false; await page.locator('#saveButton').click(); await page.waitForFunction(() => document.getElementById('saveBar').hidden);
        assert.equal(lastSave.io_pins.length, 6, 'Save configured pins only'); assert.equal(lastSave.device_note, 'Preserve me');
        assert.equal(lastSave.io_pins.find(pin => pin.gpio === 4).initial_state, 'HIGH'); assert.equal(values[4], 1);
        const e32 = await edit(32); await e32.locator('[data-field="enabled"]').check(); await e32.locator('[data-field="type"]').selectOption('ANALOG_INPUT');
        await e32.locator('[data-field="label"]').fill('Nouveau capteur'); await page.locator('#saveButton').click(); await page.waitForFunction(() => document.getElementById('configuredCount').textContent === '7');
        assert.equal(lastSave.io_pins.length, 7); assert(lastSave.io_pins.some(pin => pin.gpio === 32 && pin.type === 'ANALOG_INPUT'));
        const remove32 = await edit(32); await remove32.locator('[data-field="enabled"]').uncheck(); await page.locator('#saveButton').click(); await page.waitForFunction(() => document.getElementById('configuredCount').textContent === '6');
        assert(!lastSave.io_pins.some(pin => pin.gpio === 32));
        const discard = await edit(15); await discard.locator('[data-field="label"]').fill('À annuler'); await page.locator('#discardButton').click(); assert.equal(await row(15).locator('.pin-name').textContent(), 'LED renommée');
        await page.locator('#pinSearch').fill('capteur zéro'); assert.equal(await filtered().count(), 1); await page.locator('#pinSearch').fill('aucun nom'); assert.equal(await filtered().count(), 0); await page.locator('#pinSearch').fill('');
        const downloadEvent = page.waitForEvent('download'); await page.locator('#exportButton').click(); const download = await downloadEvent;
        assert.deepEqual(JSON.parse(fs.readFileSync(await download.path(), 'utf8')), { ...config, _revision: `fixture.${revision}` });
        await page.waitForSelector('#pinChart svg');
        rejectCommand = true; await row(15).locator('[data-value="1"]').click(); await page.waitForFunction(() => document.getElementById('actionNotice').dataset.state === 'error');
        assert.equal(values[15], 0); assert.equal(await row(15).locator('.pin-state').textContent(), '—');
        assert.equal(await card(15).locator('.output-card-state').textContent(), '—');
        assert.equal(await page.locator('.activity-event').first().getAttribute('data-source'), 'error');
        rejectCommand = false; await page.locator('#refreshButton').click(); await waitOnline();
        offline = true; await page.locator('#refreshButton').click(); await page.waitForFunction(() => document.getElementById('boardBadge').textContent === 'Board inaccessible');
        assert.equal(await row(15).locator('[data-value="1"]').isDisabled(), true); assert.equal(await row(39).locator('.pin-state').textContent(), '—');
        assert.equal(await card(15).locator('[data-value="1"]').isDisabled(), true);
        offline = false; await page.locator('#refreshButton').click(); await waitOnline();
        await page.setViewportSize({ width: 768, height: 1024 }); await noOverflow();
        await page.setViewportSize({ width: 320, height: 720 }); await noOverflow();
        await page.locator('#navToggle').click(); await page.locator('.nav-tools summary').click(); await noOverflow();
        await page.locator('#navToggle').click();
        await page.setViewportSize({ width: 1440, height: 1050 });
        // Synchronize external changes while preserving local drafts and resolving conflicts explicitly.
        await page.locator('[data-filter="all"]').click();
        config.io_pins.find(pin => pin.gpio === 4).label = 'Nom externe'; revision++;
        await page.locator('#refreshButton').click();
        await page.waitForFunction(() => document.querySelector('.pin-row[data-gpio="4"] .pin-name').textContent === 'Nom externe');
        assert.match(await row(35).locator('.pin-usage').textContent(), /batterie/);
        const localEditor = await edit(15); await localEditor.locator('[data-field="label"]').fill('Mon brouillon');
        config.io_pins.find(pin => pin.gpio === 4).label = 'Autre changement'; revision++;
        await page.locator('#refreshButton').click();
        await page.waitForFunction(() => document.querySelector('.pin-row[data-gpio="4"] .pin-name').textContent === 'Autre changement');
        assert.equal(await localEditor.locator('[data-field="label"]').inputValue(), 'Mon brouillon');
        assert.equal(await page.locator('#conflictBar').isVisible(), false);
        config.io_pins.find(pin => pin.gpio === 15).label = 'Choix distant'; revision++;
        await page.locator('#refreshButton').click(); await page.waitForSelector('#conflictBar:visible');
        assert.equal(await localEditor.locator('[data-field="label"]').inputValue(), 'Mon brouillon');
        assert.equal(await page.locator('#saveButton').isDisabled(), true);
        await page.locator('#keepDrafts').click(); await page.locator('#saveButton').click();
        await page.waitForFunction(() => document.getElementById('saveBar').hidden);
        assert.equal(config.io_pins.find(pin => pin.gpio === 15).label, 'Mon brouillon');
        // Inverted logical ON is electrical LOW in the table, cards and command response.
        const inversion = await edit(15); await inversion.locator('[data-field="inverted"]').check();
        await inversion.locator('[data-field="initial_state"]').selectOption('HIGH');
        await page.locator('#saveButton').click(); await page.waitForFunction(() => document.getElementById('saveBar').hidden);
        await card(15).locator('[data-value="1"]').click();
        await page.waitForFunction(() => document.querySelector('.io-output-card[data-gpio="15"] .output-card-state').textContent === 'ON');
        assert.equal(values[15], 0); assert.equal(await row(15).locator('.pin-state').textContent(), 'ON');
        assert.match(await card(15).locator('.output-card-level').textContent(), /LOW/);
        await page.waitForFunction(() => document.querySelector('.io-output-card[data-gpio="15"] .output-card-note').textContent.includes('Au démarrage : OFF'));
        await card(15).locator('[data-value="0"]').click();
        await page.waitForFunction(() => document.querySelector('.io-output-card[data-gpio="15"] .output-card-state').textContent === 'OFF');
        assert.equal(values[15], 1);
        await card(15).locator('.output-pulse input').fill('1'); await card(15).locator('.pulse-button').click();
        await page.waitForFunction(() => document.querySelector('.io-output-card[data-gpio="15"] .output-countdown').textContent.includes('automatique'));
        await page.waitForTimeout(1300); await page.locator('#refreshButton').click();
        await page.waitForFunction(() => document.querySelector('.io-output-card[data-gpio="15"] .output-card-state').textContent === 'OFF');
        assert.equal(values[15], 1); assert.equal(await card(15).locator('.output-countdown').textContent(), '');
        await page.screenshot({ path: path.join(artifact, 'io-six-desktop.png'), fullPage: true });
        await page.setViewportSize({ width: 390, height: 844 }); await noOverflow();
        await page.screenshot({ path: path.join(artifact, 'io-six-mobile.png'), fullPage: true });
        await page.setViewportSize({ width: 1440, height: 1050 });
        config.io_pins.push({ gpio: 21, label: 'Sortie partagée', mode: 'OUTPUT', type: 'DIGITAL' }); revision++;
        await page.locator('#refreshButton').click();
        await page.waitForFunction(() => document.getElementById('configuredCount').textContent === '7');
        assert.equal(await row(21).locator('.pin-state').textContent(), '—');
        assert.equal(await card(21).locator('[data-value="1"]').isDisabled(), true);
        assert.equal(await card(21).locator('.pulse-button').isDisabled(), true);
        assert.match(await row(21).locator('.pin-value-detail').textContent(), /I²C/);
        config.io_pins.pop(); revision++; await page.locator('#refreshButton').click();
        await page.waitForFunction(() => document.getElementById('configuredCount').textContent === '6');
        // Both malformed preferences and disabled preference storage leave the page usable.
        await page.evaluate(() => localStorage.setItem('iyak32.io.preferences', 'broken JSON')); await page.reload(); await waitOnline();
        assert.equal(await page.locator('#favoriteCount').textContent(), '0');
        await context.addInitScript(() => {
            const get = Storage.prototype.getItem, set = Storage.prototype.setItem;
            Storage.prototype.getItem = function(key) { if (key === 'iyak32.io.preferences') throw new Error('Storage unavailable'); return get.call(this, key); };
            Storage.prototype.setItem = function(key, value) { if (key === 'iyak32.io.preferences') throw new Error('Storage unavailable'); return set.call(this, key, value); };
        });
        await page.reload(); await waitOnline(); await card(15).locator('.favorite-button').click();
        assert.equal(await page.locator('#favoriteCount').textContent(), '1');
        // The board journal records both edges even when a pulse finishes between browser polls.
        const shortPulse = await fetch(base + '/api/io/set?gpio=15&active=1&duration_ms=100', { method: 'POST' });
        assert.equal(shortPulse.status, 200); const shortPulseResult = await shortPulse.json(); await page.waitForTimeout(180);
        await page.locator('#refreshButton').click();
        await page.waitForFunction(id => document.querySelector('.activity-event')?.dataset.source === 'timer' && document.querySelector('.activity-event:nth-child(2)')?.dataset.eventId === String(id), shortPulseResult.event_id);
        assert.match(await page.locator('.activity-event').first().textContent(), /Arrêt automatique/);
        assert.match(await page.locator('.activity-event').nth(1).textContent(), /Activation pour 0.1 s/);
        await page.locator('#activitySource').selectOption('mqtt');
        assert.equal(await page.locator('.activity-event').count(), 1);
        await page.locator('#activitySource').selectOption('');
        const logDownloadEvent = page.waitForEvent('download'); await page.locator('#exportActivity').click();
        const logDownload = await logDownloadEvent;
        const exported = JSON.parse(fs.readFileSync(await logDownload.path(), 'utf8'));
        assert.equal(exported.session, session); assert(exported.events.some(event => event.kind === 'expired'));
        await page.locator('#clearActivity').click(); assert.equal(await page.locator('.activity-event').count(), 0);
        await page.reload(); await waitOnline(); await page.waitForSelector('.activity-event');
        assert.equal(await page.locator('.activity-event').count(), journal.length, 'Hiding the view preserves board records');
        for (let i = 0; i < 70; i++) journalEvent(config.io_pins.find(pin => pin.gpio === 15), 'mqtt', 'command', 0);
        await page.locator('#refreshButton').click();
        await page.waitForFunction(id => document.querySelectorAll('.activity-event').length === 64 && document.querySelector('.activity-event')?.dataset.eventId === String(id), eventSequence);
        assert.match(await page.locator('#activityStatus').textContent(), /remplacé/);
        assert.equal(new Set(await page.locator('.activity-event').evaluateAll(elements => elements.map(el => el.dataset.eventId))).size, 64);
        await page.locator('#refreshButton').click(); await page.waitForTimeout(100);
        assert.equal(await page.locator('.activity-event').count(), 64, 'Polling does not duplicate events');
        journalOffline = true; await page.locator('#refreshButton').click();
        await page.waitForFunction(() => document.getElementById('activityStatus').textContent.includes('inaccessible'));
        assert.equal(await card(15).locator('[data-value="1"]').isDisabled(), false, 'Journal failure does not disable working outputs');
        journalOffline = false; session = 'next-boot'; eventSequence = 0; journal.length = 0;
        journalEvent(config.io_pins.find(pin => pin.gpio === 15), 'boot', 'startup');
        await page.locator('#refreshButton').click();
        await page.waitForFunction(() => document.querySelectorAll('.activity-event').length === 1 && document.querySelector('.activity-event').dataset.source === 'boot');
        assert.match(await page.locator('.activity-event').textContent(), /Démarrage/);
        assert.match(await page.locator('#ioDiscoveryBadge').textContent(), /6 I\/O annoncées/);
        await page.screenshot({ path: path.join(artifact, 'io-journal-desktop.png'), fullPage: true });
        await page.setViewportSize({ width: 390, height: 844 }); await noOverflow();
        await page.screenshot({ path: path.join(artifact, 'io-journal-mobile.png'), fullPage: true });
        // Favorite durations are board configuration, and Tout OFF cancels every available timer.
        await page.setViewportSize({ width: 1440, height: 1050 });
        assert.deepEqual(await card(15).locator('.pulse-presets button').evaluateAll(buttons => buttons.map(button => Number(button.dataset.duration))), [5000, 30000, 60000]);
        await card(15).locator('[data-duration="5000"]').click();
        await page.waitForFunction(() => document.querySelector('.io-output-card[data-gpio="15"] .output-card-state').textContent === 'ON');
        await page.locator('#allOff').click();
        await page.waitForFunction(() => document.querySelector('.io-output-card[data-gpio="15"] .output-card-state').textContent === 'OFF' && document.querySelector('.io-output-card[data-gpio="4"] .output-card-state').textContent === 'OFF');
        assert.equal(timers[15], 0);
        const presets = await edit(15); await presets.locator('[data-field="pulse_presets_ms"]').fill('5 ; 5');
        assert.equal(await page.locator('#saveButton').isDisabled(), true);
        await presets.locator('[data-field="pulse_presets_ms"]').fill('0.2 ; 1 ; 60');
        await page.locator('#saveButton').click(); await page.waitForFunction(() => document.getElementById('saveBar').hidden);
        assert.deepEqual(config.io_pins.find(pin => pin.gpio === 15).pulse_presets_ms, [200, 1000, 60000]);
        await page.reload(); await waitOnline();
        assert.equal(await card(15).locator('[data-duration="200"]').count(), 1);
        await card(15).locator('[data-duration="200"]').click(); await page.waitForTimeout(350); await page.locator('#refreshButton').click();
        await page.waitForFunction(() => document.querySelector('.io-output-card[data-gpio="15"] .output-card-state').textContent === 'OFF');
        // ADC conversion preserves raw zero and handles reversed endpoints, clamping, capture and nested drafts.
        const digitalPreview = await edit(34); await digitalPreview.locator('[data-field="type"]').selectOption('ANALOG_INPUT');
        await digitalPreview.locator('[data-field="calibration_enabled"]').check();
        assert.match(await digitalPreview.locator('.calibration-preview').textContent(), /Enregistrez le mode analogique/);
        assert.equal(await digitalPreview.locator('[data-capture="raw_min"]').isDisabled(), true, 'A digital level cannot be used as an ADC reading');
        await page.locator('#discardButton').click();
        const calibration = await edit(39); await calibration.locator('[data-field="calibration_enabled"]').check();
        await calibration.locator('[data-field="raw_min"]').fill('3500'); await calibration.locator('[data-field="raw_max"]').fill('500');
        assert.match(await calibration.locator('.calibration-preview').textContent(), /0 ADC → 100/);
        await calibration.locator('[data-field="clamp"]').uncheck();
        assert.match(await calibration.locator('.calibration-preview').textContent(), /116/);
        await calibration.locator('[data-field="clamp"]').check();
        await calibration.locator('[data-field="raw_max"]').fill('3500'); assert.equal(await page.locator('#saveButton').isDisabled(), true);
        await calibration.locator('[data-field="raw_max"]').fill('500'); await page.locator('#saveButton').click();
        await page.waitForFunction(() => document.getElementById('saveBar').hidden);
        await page.waitForFunction(() => document.querySelector('.pin-row[data-gpio="39"] .pin-state').textContent.includes('%'));
        assert.match(await row(39).locator('.pin-state').textContent(), /100.*%/);
        assert.match(await row(39).locator('.pin-value-detail').textContent(), /ADC brut : 0/);
        const capture = await edit(39); await capture.locator('[data-capture="raw_min"]').click();
        assert.equal(await capture.locator('[data-field="raw_min"]').inputValue(), '0');
        assert.equal(await page.locator('#saveBar').isVisible(), true); await page.locator('#discardButton').click();
        const adc2Calibration = await edit(25); await adc2Calibration.locator('[data-field="calibration_enabled"]').check();
        assert.match(await adc2Calibration.locator('.calibration-preview').textContent(), /indisponible/);
        assert.equal(await adc2Calibration.locator('[data-capture="raw_min"]').isDisabled(), true); await page.locator('#discardButton').click();
        // Scenes have explicit selected outputs, logical states and bounded board durations.
        await page.locator('#newScene').click(); await page.locator('#sceneName').fill('Scène test');
        assert.equal(await page.locator('#saveButton').isDisabled(), true, 'A new scene needs an action');
        const scene15 = page.locator('.scene-action[data-gpio="15"]'); await scene15.locator('[type="checkbox"]').check();
        await scene15.locator('[type="number"]').fill('1');
        await page.locator('.scene-action[data-gpio="4"] [type="checkbox"]').check();
        await page.locator('.scene-action[data-gpio="4"] select').selectOption('0');
        assert.equal(await page.locator('.scene-card .btn').first().isDisabled(), true);
        await page.locator('#saveButton').click(); await page.waitForFunction(() => document.getElementById('saveBar').hidden);
        assert.equal(config.scenes.length, 1); const sceneID = config.scenes[0].id;
        const sceneCard = () => page.locator(`.scene-card[data-scene-id="${sceneID}"]`);
        await sceneCard().getByRole('button', { name: 'Lancer', exact: true }).click();
        await page.waitForFunction(() => document.querySelector('.io-output-card[data-gpio="15"] .output-card-state').textContent === 'ON');
        assert.equal(values[15], 0, 'Scene ON respects output inversion'); assert.equal(values[4], 0);
        await page.waitForTimeout(1300); await page.locator('#refreshButton').click();
        await page.waitForFunction(() => document.querySelector('.activity-event')?.textContent.includes('Scène : Scène test') && document.querySelector('.activity-event')?.dataset.source === 'timer');
        assert.equal(values[15], 1); assert.equal(timers[15], 0);
        // The open editor continues to edit the current saved scene object after save/reload.
        await page.locator('#sceneName').fill('Mon brouillon scène');
        config.scenes[0].name = 'Scène externe'; revision++;
        await page.locator('#refreshButton').click(); await page.waitForSelector('#conflictBar:visible');
        assert.equal(await page.locator('#sceneName').inputValue(), 'Mon brouillon scène'); assert.equal(await page.locator('#saveButton').isDisabled(), true);
        await page.locator('#keepDrafts').click(); await page.locator('#saveButton').click(); await page.waitForFunction(() => document.getElementById('saveBar').hidden);
        assert.equal(config.scenes[0].name, 'Mon brouillon scène');
        await page.locator('#closeScene').click();
        const scenePin = await edit(15); await scenePin.locator('[data-field="enabled"]').uncheck();
        assert.equal(await page.locator('#saveButton').isDisabled(), true); assert.match(await page.locator('#draftError').textContent(), /scène lors d’un retrait/);
        await page.locator('#discardButton').click();
        // Import strips a foreign revision, validates without mutation, previews values/scenes, then stages only.
        const imported = structuredClone(config); imported._revision = 'other-board.123'; imported.device_note = 'Foreign note';
        imported.io_pins.find(pin => pin.gpio === 15).label = 'Sortie importée'; imported.io_pins.find(pin => pin.gpio === 15).pulse_presets_ms = [200, 500];
        imported.io_pins.find(pin => pin.gpio === 39).calibration.raw_min = 3200;
        imported.scenes[0].name = 'Scène importée';
        const upload = data => page.locator('#importFile').setInputFiles({ name: 'configuration.json', mimeType: 'application/json', buffer: Buffer.from(typeof data === 'string' ? data : JSON.stringify(data)) });
        const beforeImport = configurationSaves, beforeValidation = validationCount, priorOutput = values[15];
        await upload(imported); await page.waitForSelector('#importPanel:visible');
        assert.equal(configurationSaves, beforeImport); assert.equal(values[15], priorOutput); assert.equal(validationCount, beforeValidation + 1);
        assert.match(await page.locator('#importChanges').textContent(), /ADC 3200/); assert.match(await page.locator('#importChanges').textContent(), /Scène importée/);
        await page.locator('#stageImport').click(); assert.equal(configurationSaves, beforeImport);
        assert.equal(await row(15).locator('.pin-name').textContent(), 'Sortie importée'); await page.locator('#discardButton').click();
        assert.notEqual(await row(15).locator('.pin-name').textContent(), 'Sortie importée');
        await upload(imported); await page.waitForSelector('#importPanel:visible'); await page.locator('#stageImport').click();
        await page.locator('#saveButton').click(); await page.waitForFunction(() => document.getElementById('saveBar').hidden);
        assert.equal(configurationSaves, beforeImport + 1); assert.equal(config.device_note, 'Preserve me');
        assert.equal(config.scenes[0].name, 'Scène importée'); assert.equal(config.io_pins.find(pin => pin.gpio === 39).calibration.raw_min, 3200);
        assert.equal(values[15], priorOutput, 'Importing metadata preserves an existing output state');
        const invalidImport = structuredClone(config); invalidImport.io_pins.push(structuredClone(invalidImport.io_pins[0]));
        await upload(invalidImport); await page.waitForFunction(() => document.getElementById('actionNotice').textContent.includes('Import refusé'));
        assert.equal(configurationSaves, beforeImport + 1); assert.equal(await page.locator('#saveBar').isVisible(), false);
        await upload('x'.repeat(65537)); await page.waitForFunction(() => document.getElementById('actionNotice').textContent.includes('64 Ko'));
        assert.equal(configurationSaves, beforeImport + 1);
        // A configured reserved output can be seen, but cannot be selected for a new action or activated by Tout OFF.
        config.io_pins.push({ gpio: 21, label: 'I2C partagée', mode: 'OUTPUT', type: 'DIGITAL' }); revision++;
        await page.locator('#refreshButton').click(); await page.waitForFunction(() => document.getElementById('configuredCount').textContent === '7');
        await sceneCard().getByRole('button', { name: 'Modifier', exact: true }).click();
        assert.equal(await page.locator('.scene-action[data-gpio="21"] [type="checkbox"]').isDisabled(), true);
        await page.locator('#allOff').click(); await page.waitForFunction(() => document.getElementById('actionNotice').textContent.includes('GPIO 21'));
        assert.equal(values[15], 1); assert.equal(values[4], 0); assert.match(await page.locator('#allOffNote').textContent(), /21/);
        await page.locator('#closeScene').click();
        await page.screenshot({ path: path.join(artifact, 'io-actions-desktop.png'), fullPage: true });
        await page.setViewportSize({ width: 390, height: 844 }); await noOverflow();
        await edit(39); await noOverflow(); await page.screenshot({ path: path.join(artifact, 'io-calibration-mobile.png'), fullPage: true });
        await page.setViewportSize({ width: 320, height: 720 }); await noOverflow();
        assert(await page.locator('#editor-39 .calibration-fields').evaluate(fieldset => {
            const bounds = fieldset.getBoundingClientRect();
            return [...fieldset.querySelectorAll('input, button')].every(input => { const box = input.getBoundingClientRect(); return box.left >= bounds.left && box.right <= bounds.right + 1; });
        }), 'Calibration controls must fit inside their panel, not be clipped');
        await page.locator('#editor-39 .calibration-fields').screenshot({ path: path.join(artifact, 'io-calibration-320.png') });
        await sceneCard().getByRole('button', { name: 'Modifier', exact: true }).click(); await noOverflow();
        await page.screenshot({ path: path.join(artifact, 'io-scenes-mobile.png'), fullPage: true });
        assert(sceneRequests >= 3);
        assert.deepEqual(errors, []); assert.deepEqual(external, []);
        console.log('PASS: 26 GPIO, quick output commands, saved favorites/filters, external configuration merge/conflicts, active-low outputs, timed activation and reserved GPIO, board journal reload/origins, short pulses, bounded history, export/filter/hide, reboot reset and journal outage isolation, change preview, live zero/ADC2/effective-mode states, explicit idempotent ON/OFF, draft preservation, save rejection/recovery, configured-only saves, add/remove/discard/export/history, desktop/mobile, optional storage and offline recovery; favorite durations, all-OFF timer cancellation, reversed ADC calibration/capture, scene persistence/inversion/timers/conflicts, referenced pin validation, import validation/preview/staging/discard/apply and reserved scene targets; no external requests or browser exceptions');
    } finally { await browser.close(); server.close(); }
})().catch(error => { console.error(error); server.close(); process.exitCode = 1; });
