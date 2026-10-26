// Run with Playwright available to Node; the browser uses fixture APIs only.
const assert = require('node:assert/strict');
const http = require('node:http');
const fs = require('node:fs');
const path = require('node:path');
const { chromium } = require('playwright');
const root = path.resolve(__dirname, '../data');
const artifact = process.env.UI_ARTIFACT_DIR || '/tmp/iyak32-ui';
fs.mkdirSync(artifact, { recursive: true });
let offline = false, rejectCommand = false;
const outputs = { 21: 0, 4: 0, 15: 0 };
const status = { device_name: 'ESP_TEST01', mode: 'basic', version: 'v1:10-ui', uptime: 7200,
    wifi_connected: true, wifi_ssid: 'Fixture Wi-Fi', wifi_ip: '192.0.2.15', wifi_rssi: -72,
    mqtt_enabled: true, mqtt_state: 'connected', bmx_temp: 26.5, bmx_pressure: 1017.5,
    battery: 4.25, cpu_temp: 64.4, free_heap: 180000, sample_interval_sec: 5, sensor_age_ms: 500 };
const rows = Array.from({ length: 120 }, (_, i) => ({ t: 6600000 + i * 5000,
    temp: 26.4 + Math.sin(i / 15) * .2, pres: 1017.5 + Math.sin(i / 20) * .15,
    hum: null, batt: 4.25, heap: 180000, rssi: -72 + Math.round(Math.sin(i / 8)) }));
const pins = Object.keys(outputs).map(gpio => ({ gpio: Number(gpio), label: 'LED ' + gpio, mode: 'OUTPUT', type: 'DIGITAL' }));
const traffic = { sent: 420, received: 3, failed: 0, topics: [
    { topic: 'sensors/ESP_TEST01/temperature', payload: '26.5', direction: 'out', ok: true, age_ms: 500 },
    { topic: 'esp32/data/ESP_TEST01', payload: JSON.stringify({ payload: { bmx_temp_c: 26.5 } }), direction: 'out', ok: true, age_ms: 500 }
] };
const server = http.createServer((request, response) => {
    const url = new URL(request.url, 'http://localhost');
    const reply = (data, code = 200) => { response.writeHead(code, { 'Content-Type': 'application/json' }); response.end(JSON.stringify(data)); };
    if (url.pathname.startsWith('/api/')) {
        if (offline) return reply({ error: 'Board unavailable' }, 503);
        switch (url.pathname) {
            case '/api/status': return reply(status);
            case '/api/mqtt/status': return reply({ enabled: true, connected: true, state: 'connected', server: '192.0.2.10', port: 1883 });
            case '/api/mqtt/traffic': return reply(traffic);
            case '/api/mqtt/reconnect': return reply({ status: 'ok' });
            case '/api/logs/sensors': return reply(rows);
            case '/api/io/config': return reply({ io_pins: pins });
            case '/api/io/status': return reply({ statuses: Object.entries(outputs).map(([gpio, value]) => ({ gpio: Number(gpio), value })) });
            case '/api/io/set': {
                if (rejectCommand) return reply({ message: 'Commande refusée' }, 400);
                const gpio = url.searchParams.get('gpio'); outputs[gpio] = Number(url.searchParams.get('active')); return reply({ status: 'success' });
            }
            case '/api/io/toggle': {
                if (rejectCommand) return reply({ message: 'Commande refusée' }, 400);
                const gpio = url.searchParams.get('gpio'); outputs[gpio] = outputs[gpio] ? 0 : 1; return reply({ status: 'success' });
            }
            case '/api/basicmode/status': return reply({ led1: !!outputs[21], led2: !!outputs[4], led3: !!outputs[15], sound: false });
            case '/api/basicmode/analogstatus': return reply({ analog1_value: 0, analog2_value: 0, analog1_threshold: 2000, analog2_threshold: 2000 });
            case '/api/basicmode/sound': case '/api/basicmode/threshold':
                return reply(rejectCommand ? { message: 'Commande refusée' } : { status: 'ok' }, rejectCommand ? 400 : 200);
        }
        return reply({ error: 'Not found' }, 404);
    }
    let file = url.pathname === '/' ? '/index.html' : url.pathname;
    if (file.endsWith('/')) file += 'index.html';
    const resolved = path.join(root, file);
    if (!resolved.startsWith(root + path.sep) || !fs.existsSync(resolved)) { response.writeHead(404); response.end(); return; }
    response.setHeader('Content-Type', file.endsWith('.js') ? 'text/javascript' : file.endsWith('.css') ? 'text/css' : 'text/html');
    response.end(fs.readFileSync(resolved));
});

(async () => {
    await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
    const browser = await chromium.launch({ headless: true, ...(process.env.CHROMIUM_PATH ? { executablePath: process.env.CHROMIUM_PATH } : {}) });
    try {
        const context = await browser.newContext({ viewport: { width: 1440, height: 1100 }, acceptDownloads: true });
        // LAN HTTP cannot use the clipboard API; exercise the download fallback.
        await context.addInitScript(() => Object.defineProperty(navigator, 'clipboard', { value: undefined }));
        const page = await context.newPage(), errors = [], external = [];
        page.on('pageerror', e => errors.push(e.message));
        page.on('request', r => { if (!r.url().startsWith(base)) external.push(r.url()); });
        const base = `http://127.0.0.1:${server.address().port}`;
        const noOverflow = async () => assert(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth), 'Horizontal overflow');
        await page.goto(base); await page.waitForFunction(() => document.getElementById('temperature').textContent !== '—');
        assert.equal(await page.locator('.output-button').count(), 3);
        assert.equal(await page.locator('#humidityMetric').isVisible(), false);
        await page.screenshot({ path: path.join(artifact, 'dashboard-desktop.png'), fullPage: true }); await noOverflow();
        await page.locator('[data-gpio="15"]').click();
        await page.waitForFunction(() => document.querySelector('[data-gpio="15"]').dataset.on === 'true');
        assert.match(await page.locator('#actionNotice').textContent(), /allumée/);
        rejectCommand = true; await page.locator('[data-gpio="4"]').click();
        await page.waitForFunction(() => document.getElementById('actionNotice').dataset.state === 'error');
        assert.equal(await page.locator('[data-gpio="4"]').isDisabled(), true); rejectCommand = false;
        status.bmx_temp = 0; await page.locator('#refreshButton').click();
        await page.waitForFunction(() => document.getElementById('temperature').textContent.startsWith('0'));
        offline = true; await page.locator('#refreshButton').click();
        await page.waitForFunction(() => document.getElementById('boardBadge').textContent === 'Board inaccessible');
        assert.equal(await page.locator('[data-gpio="15"]').getAttribute('data-on'), 'unknown');
        offline = false; status.bmx_temp = 26.5; await page.locator('#refreshButton').click();
        await page.waitForFunction(() => document.getElementById('boardBadge').textContent === 'Board en ligne');
        await page.setViewportSize({ width: 390, height: 844 }); await noOverflow();
        await page.locator('#navToggle').click(); assert.equal(await page.locator('#navToggle').getAttribute('aria-expanded'), 'true');
        await page.screenshot({ path: path.join(artifact, 'dashboard-mobile.png'), fullPage: true });
        await page.setViewportSize({ width: 1440, height: 1100 });
        await page.goto(base + '/mqtt.html'); await page.waitForSelector('.topic-card');
        await page.locator('#reconnectButton').click();
        await page.waitForFunction(() => !document.getElementById('reconnectButton').disabled);
        await page.locator('#topicFilter').fill('temperature'); assert.equal(await page.locator('.topic-card').count(), 1);
        await page.locator('#topicFilter').fill(''); await page.locator('.topic-card summary').first().click();
        await page.locator('#pauseButton').click(); assert.equal(await page.locator('#pauseButton').textContent(), 'Reprendre');
        await page.screenshot({ path: path.join(artifact, 'mqtt-desktop.png'), fullPage: true });
        const mqttDownload = page.waitForEvent('download'); await page.locator('#copyButton').click();
        const jsonDownload = await mqttDownload; assert.equal(jsonDownload.suggestedFilename(), 'iyak32-mqtt.json');
        await page.goto(base + '/history.html'); await page.waitForSelector('.chart svg');
        assert.equal(await page.locator('#historyCharts .panel').count(), 4);
        await page.locator('[data-range="0"]').click();
        const downloaded = page.waitForEvent('download'); await page.locator('#exportButton').click();
        const download = await downloaded; const csv = fs.readFileSync(await download.path(), 'utf8');
        assert.equal(csv.split('\r\n').length, 121); assert(csv.startsWith('uptime_ms,temperature_c'));
        await page.screenshot({ path: path.join(artifact, 'history-desktop.png'), fullPage: true });
        for (const route of ['/mqtt.html', '/history.html']) {
            await page.setViewportSize({ width: 390, height: 844 }); await page.goto(base + route);
            await page.waitForTimeout(300); await noOverflow();
        }
        await page.goto(base + '/basic_mode/'); await page.waitForFunction(() => document.getElementById('basicConnection').textContent.startsWith('Board connecté'));
        assert(await page.locator('#led3_status').evaluate(el => el.classList.contains('led-on')));
        rejectCommand = true; await page.locator('.switch').click();
        await page.waitForFunction(() => document.getElementById('actionNotice').dataset.state === 'error');
        rejectCommand = false; offline = true; await page.waitForFunction(() => document.getElementById('basicConnection').textContent.includes('inaccessible'), { timeout: 10000 });
        assert(await page.locator('#led3_status').evaluate(el => el.classList.contains('led-unknown')));
        assert.equal(await page.locator('[data-led="15"]').isDisabled(), true);
        assert.deepEqual(errors, []); assert.deepEqual(external, []);
        console.log('PASS: desktop/mobile UI, zero readings, unavailable states, command failures, MQTT filter/pause/export, SVG history and CSV; no external requests or browser exceptions');
    } finally { await browser.close(); server.close(); }
})().catch(e => { console.error(e); server.close(); process.exitCode = 1; });
