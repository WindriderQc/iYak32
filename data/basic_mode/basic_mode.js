let refreshBasic, basicBusy = false;
function basicLed(element, on) {
    element.classList.toggle('led-on', on === true);
    element.classList.toggle('led-off', on === false);
    element.classList.toggle('led-unknown', on == null);
    element.setAttribute('role', 'img');
    element.setAttribute('aria-label', on == null ? 'État inconnu' : on ? 'Allumée' : 'Éteinte');
}
async function refreshBasicState() {
    const ui = BoardUI;
    const [state, analog] = await Promise.allSettled([ui.json('/api/basicmode/status'), ui.json('/api/basicmode/analogstatus')]);
    const data = state.status === 'fulfilled' ? state.value : null;
    [1, 2, 3].forEach(i => basicLed(document.getElementById('led' + i + '_status'), data ? data['led' + i] : null));
    ui.text('basicConnection', data ? 'Board connecté · état actuel des sorties' : 'Board inaccessible · état inconnu');
    document.querySelectorAll('[data-led]').forEach(button => { button.disabled = !data || data[{21: 'led1', 4: 'led2', 15: 'led3'}[button.dataset.led]] == null || basicBusy; });
    document.getElementById('soundToggle').disabled = !data || basicBusy;
    if (data && !basicBusy) document.getElementById('soundToggle').checked = data.sound;
    [1, 2].forEach(i => {
        const slider = document.getElementById('thresh' + i);
        slider.disabled = analog.status !== 'fulfilled' || basicBusy;
        if (analog.status === 'fulfilled') {
            ui.text('analog' + i + '_value', analog.value['analog' + i + '_value']);
            ui.text('analog' + i + '_threshold', analog.value['analog' + i + '_threshold']);
            if (!basicBusy && document.activeElement !== slider) slider.value = analog.value['analog' + i + '_threshold'];
        } else {
            ui.text('analog' + i + '_value', 'État inconnu');
            ui.text('analog' + i + '_threshold', '—');
        }
    });
}
async function basicCommand(path, body, label) {
    if (basicBusy) return;
    basicBusy = true;
    document.querySelectorAll('[data-led], #soundToggle, input[type=range]').forEach(el => { el.disabled = true; });
    BoardUI.notice('Commande en cours…');
    try {
        const options = { method: 'POST' };
        if (body) { options.headers = { 'Content-Type': 'application/json' }; options.body = JSON.stringify(body); }
        await BoardUI.json(path, options);
        BoardUI.notice(label);
    } catch (e) { BoardUI.notice('Commande non confirmée : ' + e.message, true); }
    finally { basicBusy = false; await refreshBasic(); }
}
function toggleLed(gpio) { return basicCommand('/api/io/toggle?gpio=' + gpio, null, 'Commande appliquée.'); }
function toggleSound() { return basicCommand('/api/basicmode/sound', { enabled: document.getElementById('soundToggle').checked }, 'Réglage sonore appliqué.'); }
function setThreshold(channel, value) { return basicCommand('/api/basicmode/threshold', { channel, value: Number(value) }, 'Seuil appliqué.'); }
document.addEventListener('DOMContentLoaded', () => { refreshBasic = BoardUI.poll(refreshBasicState); });
